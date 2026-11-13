#include "window.h"
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include "text.h"
#include "layer.h"
#include <stdio.h>

window_t* g_focused_window;

static window_t* window_create_init(int x, int y, int width, int height, char *title, unsigned int bg_color, unsigned int fg_color, int type) {
    size_t struct_size = sizeof(window_t);
    window_t* win = mmap(NULL, struct_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (win == MAP_FAILED) return NULL;
    
    memset(win, 0, struct_size);
    win->type = type;
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    win->focused = 0;
    win->client.x = 0;
    win->client.y = 0;
    win->client.width = win->width;
    win->client.height = win->height; 
    win->visible = 1;
    win->dragging = 0; 
    strncpy(win->title, title ? title : "", sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = '\0';

    if((bg_color & 0xFF000000) != 0xFF000000) win->flags |= WINDOW_FLAG_TRANSPARENT; 


    win->struct_size = struct_size;

    size_t num_pixels = (size_t)width * height;
    size_t buffer_size = sizeof(unsigned int) * num_pixels;
    win->buffer_size = buffer_size;

    win->buffer = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (win->buffer == MAP_FAILED) {
        munmap(win, struct_size);
        return NULL;
    }

    win->invalid = 0;
    win->event_head = 0;
    win->event_tail = 0;
    win->lock = 0;

    win->last_mouse_x = -1;
    win->last_mouse_y = -1;

    for (int i = 0; i < MAX_EVENTS; i++) {
        win->event_queue[i].type = EVENT_NONE;
    }

    win->bg = bg_color;
    win->fg = fg_color;

    win->pid = (unsigned long) getpid();

    unsigned char *free_font_buffer = 0;
    win->font = malloc(sizeof(stbtt_fontinfo));
    if (!win->font) {
        munmap(win->buffer, buffer_size);
        munmap(win, struct_size);
        return NULL;
    }

    memset(win->font, 0, sizeof(stbtt_fontinfo));
    if (stbtt_font_init(win->font, "usr/share/fonts/Ubuntu.ttf", &free_font_buffer) == 0) {
        win->free_font_buffer = free_font_buffer;
    } else {
        free(win->font);
        munmap(win->buffer, buffer_size);
        munmap(win, struct_size);
        return NULL;
    }

    return win;
}

window_t* window_create(int x, int y, int width, int height, char *title, unsigned int bg_color, unsigned int fg_color) {
    
    window_t* win = window_create_init(x, y, width, height, title, bg_color, fg_color, 1);
    if (!win) return NULL;
    
    int border_thickness = 2;
    win->client.x = border_thickness;
    win->client.y = WINDOW_TITLEBAR_HEIGHT + win->client.x;
    win->client.width = win->width - (win->client.x*border_thickness);
    win->client.height = win->height - (WINDOW_TITLEBAR_HEIGHT+(win->client.x*border_thickness)+(win->client.x*border_thickness*2));

    unsigned int border_color = 0xAAAAAA;  // Cinza claro elegante
    unsigned int title_color = 0x888888;
    
    for (int yy = 0; yy < height; ++yy) {
        for (int xx = 0; xx < width; ++xx) {
            int index = yy * width + xx;

            // Borda
            if (xx < border_thickness || xx >= width - border_thickness ||
                yy < border_thickness || yy >= height - border_thickness) {
                win->buffer[index] = border_color;
            }

            // Barra de título
            else if (yy < WINDOW_TITLEBAR_HEIGHT) {
                win->buffer[index] = title_color;
            }

            // Conteúdo
            else {
                win->buffer[index] = win->bg;
            }
        }
    }

    // Desenhar o título centralizado
    if (win->title[0]) {
        title_color =  0xFFFFFF; //win->focused ? 0xF0F0F0 : 0x000000;  // branco se focado, preto se não
        draw_text(*(stbtt_fontinfo*)win->font, win->title, border_thickness*2, border_thickness, win->width - (border_thickness*2), title_color, 18, win);
    }

    return win;
}

window_t* overlay_window_create(int x, int y, int width, int height, unsigned int bg_color, unsigned int fg_color) {
    
    window_t* win = window_create_init(x, y, width, height, NULL, bg_color, fg_color, 0);

    if (!win) return NULL;

    size_t num_pixels = (size_t)width * height;

    for (size_t i = 0; i < num_pixels; i++) {
        win->buffer[i] = win->bg;
    }

    return win;
}

void window_destroy(window_t *win)
{
    printf("x");
    if (!win || win == MAP_FAILED)
        return;

    if (win->buffer && win->buffer != MAP_FAILED)
        munmap(win->buffer, win->buffer_size);

    if (win->font)
        free(win->font);

    if (win->free_font_buffer)
        free(win->free_font_buffer);

    munmap(win, win->struct_size);
}


window_t* get_focused_window() {
    return g_focused_window;
}

void window_update_focus(window_t* win, int focused) {
    if (!win) return;
    win->focused = focused;

    event_t ev_tmp = {0};
    ev_tmp.type = !focused ? EVENT_FOCUS_LOST : EVENT_FOCUS_GAINED;
    ev_tmp.clock = 0;
    push_event_to_window(win, ev_tmp);
}

void window_handle_mouse(window_t* win, int layer, int delta_x, int delta_y, int buttons)
{
    int x = delta_x - win->x;
    int y = delta_y - win->y;

    if (!(buttons & 0x03))
        return;

    window_t* win_x = NULL;

    if (g_focused_window != win)
    {
        win_x = g_focused_window;

        g_focused_window = win;
        window_update_focus(win, 1);

        if (win->type == 1)
            move_window_to_top(1, win);
    }
    
    if (win_x)
        window_update_focus(win_x, 0);

    if (layer == 1 &&
        (buttons & 0x01) &&
        !win->dragging &&
        y < 20)
    {
        win->dragging = 1;
        win->drag_offset_x = x;
        win->drag_offset_y = y;
    }
}

void window_handle_key(window_t* win, int scancode) {
    (void)win;
    (void)scancode;
    // Exemplo: poderia responder a teclas
}