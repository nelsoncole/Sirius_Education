#include "compositor.h"
#include "framebuffer.h"
#include "window.h"
#include "layer.h"
#include "event.h"
#include "mouse_cursor.h"
#include "pixel.h"
#include <string.h>

#include "cpu_memory.h"

extern unsigned int* back_buffer;
extern int screen_width;
extern int screen_height;
extern int pitch;

volatile int compositor_lock = 0;

// Inicializa o compositor
void compositor_init() {
    memset(layers, 0, sizeof(layers));
    memset(layer_counts, 0, sizeof(layer_counts));
    g_focused_window = (window_t*)0;
}

/*
    Camada 0: fundo (background)
    Camada 1: janelas normais
    Camada 2: janelas sobrepostas (pop-ups, menus, etc)
*/
// Adiciona uma janela a uma camada
void compositor_add_window(window_t* win, int layer) {
    if(!win) return;
    co_lock(&compositor_lock);

    if (layer < LAYER_COUNT && layer_counts[layer] < MAX_WINDOWS_PER_LAYER) {
        layers[layer][layer_counts[layer]++] = win;

        window_update_focus(g_focused_window, 0);
        g_focused_window = win;
        window_update_focus(win, 1);
    }

    co_unlock(&compositor_lock);
}

void compositor_add_window_child(window_t* win, window_t* parent) {
    if(!win) return;
    co_lock(&compositor_lock);
    parent->parent = win;
    co_unlock(&compositor_lock);
}

void compositor_remove_window_child(window_t* win) {
    if(!win) return;
    co_lock(&compositor_lock);
    win->parent = 0;
    co_unlock(&compositor_lock);
}

void compositor_remove_window(window_t* win) {
    if (!win) return;
    co_lock(&compositor_lock);
    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        for (int i = 0; i < layer_counts[layer]; i++) {
            if (layers[layer][i] == win) {
                // Remover foco se for a janela ativa
                if (g_focused_window == win) {
                    window_update_focus(win, 0);
                    g_focused_window = NULL;
                }

                // Deslocar janelas acima para baixo
                for (int j = i; j < layer_counts[layer] - 1; j++) {
                    layers[layer][j] = layers[layer][j + 1];
                }

                layer_counts[layer]--;
                layers[layer][layer_counts[layer]] = NULL;

                // Se não tinha foco, atribuir para a última da camada mais alta
                if (!g_focused_window) {
                    for (int l = LAYER_COUNT - 1; l >= 0; l--) {
                        if (layer_counts[l] > 0) {
                            g_focused_window = layers[l][layer_counts[l] - 1];
                            window_update_focus(g_focused_window, 1);
                            break;
                        }
                    }
                }
                co_unlock(&compositor_lock);
                return;
            }
        }
    }
    co_unlock(&compositor_lock);
}

void compositor_draw_window(window_t* win) {
    
    if(!win) return;
    if (!win->visible) return;

    int max_shadow_radius = 6;

    window_lock(&win->lock);

    // =========================
    // SOMBRA (inalterado)
    // =========================
    if(win->type == 1)
    for (int r = max_shadow_radius; r >= 1; r--) {

        float alpha = (float)r / max_shadow_radius;

        unsigned char c = (unsigned char)(alpha * 50);
        unsigned int shadow_color = (c << 16) | (c << 8) | c;

        // sombra à direita
        for (int y = 6; y < win->height; y++) {
            for (int x = 0; x < r; x++) {

                int sx = win->x + win->width + x;
                int sy = win->y + y;

                if (sx >= 0 && sx < screen_width && sy >= 0 && sy < screen_height) {
                    framebuffer_blend_pixel(sx, sy, shadow_color, alpha * 0.5f);
                }
            }
        }

        // sombra embaixo
        for (int y = 0; y < r; y++) {
            for (int x = 6; x < win->width + r; x++) {

                int sx = win->x + x;
                int sy = win->y + win->height + y;

                if (sx >= 0 && sx < screen_width && sy >= 0 && sy < screen_height) {
                    framebuffer_blend_pixel(sx, sy, shadow_color, alpha * 0.5f);
                }
            }
        }
    }

    // =========================
    // CÓPIA DA JANELA
    // =========================

    int len = win->width * 4; // bytes por linha (sem alinhamento)

    // clipping à direita
    int overflow = (win->x + win->width) - screen_width;
    if (overflow > 0) {
        len -= overflow * 4;
    }
    // usar ponteiros corretamente
    unsigned int *s1 = back_buffer;      // destino
    unsigned int *s2 = win->buffer;      // origem

    // usar pitch (stride real da framebuffer)
    int stride = pitch / 4;

    // posiciona no início da linha Y da janela
    s1 += win->y * stride;

    // =========================
    // LOOP DAS LINHAS
    // =========================
    for (int y = 0; y < win->height; y++) {

        int fy = win->y + y;

        // clipping vertical
        if (fy >= 0 && fy < screen_height) {

            // clipping horizontal simples (mantendo tua lógica)
            if (win->x < screen_width) {

                // copia linha inteira da janela para framebuffer
                optimized_memcpy(s1 + win->x, s2, len);
            }
        }

        // AVANÇA usando STRIDE (pitch), NÃO screen_width
        s1 += stride;

        // origem continua usando largura da janela (correto)
        s2 += win->width;
    }

    window_unlock(&win->lock);
}


void compositor_draw_cursor(unsigned int *buffer)
{
    if (!back_buffer || !buffer)
        return;

    if (!g_mouse_cursor.visible ||
        !g_mouse_cursor.buffer ||
        g_mouse_cursor.width <= 0 ||
        g_mouse_cursor.height <= 0)
    {
        return;
    }

    if (g_mouse_cursor.width > CURSOR_W ||
        g_mouse_cursor.height > CURSOR_H)
        return;

    const int cx = g_mouse_cursor.x;
    const int cy = g_mouse_cursor.y;
    const int c_width = g_mouse_cursor.width;
    const int c_height = g_mouse_cursor.height;

    for (int y = 0; y < c_height; y++)
    {
        for (int x = 0; x < c_width; x++)
        {
            int screen_x = cx + x;
            int screen_y = cy + y;

            if (screen_x < 0 || screen_x >= screen_width ||
                screen_y < 0 || screen_y >= screen_height)
                continue;

            unsigned int cursor_color =
                g_mouse_cursor.buffer[y * c_width + x];

            unsigned int alpha = cursor_color >> 24;

            if (!alpha)
                continue;

            if (alpha == 255)
            {
                draw_pixel(
                    screen_x,
                    screen_y,
                    screen_width,
                    screen_height,
                    cursor_color,
                    buffer
                );
            }
            else
            {
                unsigned int bg_color = get_pixel(
                    screen_x,
                    screen_y,
                    screen_width,
                    screen_height,
                    back_buffer
                );

                unsigned int blended_color =
                    blend_color(bg_color, cursor_color, alpha);

                blended_color |= 0xFF000000;

                draw_pixel(
                    screen_x,
                    screen_y,
                    screen_width,
                    screen_height,
                    blended_color,
                    buffer
                );
            }
        }
    }
}

void compositor_draw_cursor_2(unsigned int *buffer)
{
    if (!back_buffer || !buffer)
        return;

    if (!g_mouse_cursor.visible ||
        !g_mouse_cursor.buffer ||
        g_mouse_cursor.width <= 0 ||
        g_mouse_cursor.height <= 0)
    {
        return;
    }

    if (g_mouse_cursor.width > CURSOR_W ||
        g_mouse_cursor.height > CURSOR_H)
        return;

    const int cx = g_mouse_cursor.x;
    const int cy = g_mouse_cursor.y;
    const int c_width = g_mouse_cursor.width;
    const int c_height = g_mouse_cursor.height;

    for (int y = 0; y < c_height; y++)
    {
        for (int x = 0; x < c_width; x++)
        {
            int screen_x = cx + x;
            int screen_y = cy + y;

            if (screen_x < 0 || screen_x >= screen_width ||
                screen_y < 0 || screen_y >= screen_height)
                continue;

            unsigned int cursor_color =
                g_mouse_cursor.buffer[y * c_width + x];

            unsigned int alpha = cursor_color >> 24;

            if (!alpha)
                continue;

            if (alpha == 255)
            {
                draw_pixel(
                    x,
                    y,
                    c_width,
                    c_height,
                    cursor_color,
                    buffer
                );
            }
            else
            {
                unsigned int bg_color = get_pixel(
                    screen_x,
                    screen_y,
                    screen_width,
                    screen_height,
                    back_buffer
                );

                unsigned int blended_color =
                    blend_color(bg_color, cursor_color, alpha);

                blended_color |= 0xFF000000;

                draw_pixel(
                    x,
                    y,
                    c_width,
                    c_height,
                    blended_color,
                    buffer
                );
            }
        }
    }
}

// Handlers
void compositor_handle_key_down(int key, int type) {
    window_t* win = get_focused_window();
    if (!win) return;

    event_t ev;
    if(!type) {
        ev.type = EVENT_TEXT_INPUT;
        ev.text.codepoint = key;
        ev.key.mods = 0;//get_keyboard_modifiers(); // Ex: Shift/Ctrl
    }else {
        ev.type = EVENT_KEY_DOWN;
        ev.key.keycode = key;
        ev.key.mods = 0;//get_keyboard_modifiers(); // Ex: Shift/Ctrl
    }

    push_event_to_window(win, ev);

}



// Redesenha todas as janelas de todas as camadas
void compositor_render() {
    //framebuffer_clear(rgba(97, 96, 96, 1));

    co_lock(&compositor_lock);

    for (int layer = 0; layer < LAYER_COUNT; layer++) {
        for (int i = 0; i < layer_counts[layer]; i++) {
            window_t *win = layers[layer][i];
            
            compositor_draw_window(win);
            window_t *parent = win->parent;
            while(parent){
                compositor_draw_window(parent);
                parent = parent->parent;        
            }
        }

    }

    compositor_draw_cursor(back_buffer);

    co_unlock(&compositor_lock);
}


// Composer remover janelas com threads invalidas
void compositor_remove_invalid_window()
{
    /*for (int layer = 0; layer < LAYER_COUNT; layer++) {

        int i = 0;

        while (i < layer_counts[layer]) {

            window_t *win = layers[layer][i];

            if (!win || !process_exists(win->pid)) {

                compositor_remove_window(win);

                continue;
            }

            i++;
        }
    }*/
}