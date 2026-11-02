#include "window.h"
#include <string.h>
#include <sys/mman.h>

window_t* window_create(int x, int y, int width, int height, char *title, unsigned int bg_color, unsigned int fg_color) {
    size_t struct_size = sizeof(window_t);
    window_t* win = mmap(NULL, struct_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (win == MAP_FAILED) return NULL;
    
    memset(win, 0, struct_size);
    win->type = 1;
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    win->focused = 0;
    win->client.x = 1;
    win->client.y = 20 + 1;
    win->client.width = win->width - 2;
    win->client.height = win->height - (20+2+4); 
    win->visible = 1;
    win->dragging = 0; 
    strncpy(win->title, title ? title : "", sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = '\0';

    size_t num_pixels = (size_t)width * height;
    size_t buffer_size = sizeof(unsigned int) * num_pixels;

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

    for (size_t i = 0; i < num_pixels; i++) {
        win->buffer[i] = win->bg;
    }

    return win;
}

void window_destroy(window_t* win) {
    if (!win || win == MAP_FAILED) return;
    size_t buffer_size = sizeof(unsigned int) * win->width * win->height;
    if (win->buffer != MAP_FAILED) munmap(win->buffer, buffer_size);
    munmap(win, sizeof(window_t));
}