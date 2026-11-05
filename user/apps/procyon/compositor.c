#include "compositor.h"
#include "framebuffer.h"
#include "window.h"
#include "layer.h"
#include "event.h"
#include <string.h>

#include "cpu_memory.h"

extern unsigned int* back_buffer;
extern int screen_width;
extern int screen_height;
extern int pitch;

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

    if (layer < LAYER_COUNT && layer_counts[layer] < MAX_WINDOWS_PER_LAYER) {
        layers[layer][layer_counts[layer]++] = win;

        window_update_focus(g_focused_window, 0);
        g_focused_window = win;
        window_update_focus(win, 1);
    }
}

void compositor_add_window_child(window_t* win, window_t* parent) {
    if(!win) return;

    win->parent = parent;
}

void compositor_remove_window_child(window_t* win) {
    if(!win) return;

    win->parent = 0;
}

void compositor_remove_window(window_t* win) {
    if (!win) return;

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

                return;
            }
        }
    }

}

void compositor_draw_window(window_t* win) {
    if(!win) return;
    if (!win->visible) return;

    int max_shadow_radius = 6;

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
}

void compositor_draw_cursor(void)
{
    /*if (!g_mouse_cursor.visible ||
        !g_mouse_cursor.buffer ||
        g_mouse_cursor.width <= 0 ||
        g_mouse_cursor.height <= 0)
        return;

    int cx = g_mouse_cursor.x;
    int cy = g_mouse_cursor.y;

    int width = g_mouse_cursor.width;
    int height = g_mouse_cursor.height;

    int fb_pitch = pitch / 4;

    int start_x = cx < 0 ? -cx : 0;
    int start_y = cy < 0 ? -cy : 0;

    int end_x = width;
    int end_y = height;

    if (cx + end_x > screen_width)
        end_x = screen_width - cx;

    if (cy + end_y > screen_height)
        end_y = screen_height - cy;

    if (start_x >= end_x || start_y >= end_y)
        return;

    unsigned int *cursor = g_mouse_cursor.buffer;
    unsigned int *fb = back_buffer;

    for (int y = start_y; y < end_y; y++)
    {
        int fy = cy + y;

        unsigned int *src = cursor + y * width + start_x;
        unsigned int *dst = fb + fy * fb_pitch + cx + start_x;

        for (int x = start_x; x < end_x; x++, src++, dst++)
        {
            unsigned int color = *src;

            unsigned int a = color >> 24;

            if (a == 0)
                continue;

            if (a == 255)
            {
                *dst = color & 0x00FFFFFF;
                continue;
            }

            unsigned int bg = *dst;

            unsigned int r = (((color >> 16) & 0xFF) * a +
                              ((bg >> 16) & 0xFF) * (255 - a)) / 255;

            unsigned int g = (((color >> 8) & 0xFF) * a +
                              ((bg >> 8) & 0xFF) * (255 - a)) / 255;

            unsigned int b = ((color & 0xFF) * a +
                              (bg & 0xFF) * (255 - a)) / 255;

            *dst = (r << 16) | (g << 8) | b;
        }
    }*/
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

    
    /*if (!mouse_callback) {
        compositor_draw_cursor();
    }*/
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