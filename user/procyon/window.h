
#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include <stddef.h>

#include "event.h"

#define WINDOW_TITLE_MAX 256
#define WINDOW_TITLEBAR_HEIGHT 30

#define WINDOW_FLAG_TRANSPARENT  0x01

typedef struct {
    int x, y;
    int width, height;
} rect_t;

typedef struct {
    int x, y;
    int width, height;
    unsigned int fg;
    unsigned int bg;
} client_t;

typedef struct window_t {
    int type;
    uint32_t flags;

    int x;
    int y;
    int width;
    int height;

    unsigned int fg;
    unsigned int bg;

    int focused;
    int visible;

    unsigned int *buffer;

    int dragging;
    int drag_offset_x;
    int drag_offset_y;

    char title[WINDOW_TITLE_MAX];

    client_t client;

    volatile int event_head;
    volatile int event_tail;
    event_t event_queue[MAX_EVENTS];

    int last_mouse_x;
    int last_mouse_y;

    int invalid;

    volatile int lock;  /* 0 = Pronto para ler, 1 = Cliente a desenhar */

    unsigned long pid;

    void *font;
    void* free_font_buffer;

    struct window_t *parent;

    void *wait;

    size_t buffer_size;
    size_t struct_size;

}window_t;

static inline void window_lock(volatile int* lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
         while (*lock); // espera ativa (menos agressivo)
    }
}

static inline void window_unlock(volatile int* lock) {
    __sync_lock_release(lock);
}

window_t* window_create(int x, int y, int width, int height, char *title, unsigned int bg_color, unsigned int fg_color);
window_t* overlay_window_create(int x, int y, int width, int height, unsigned int bg_color, unsigned int fg_color);
void window_destroy(window_t* win);
window_t* get_focused_window();
void window_update_focus(window_t* win, int focused);
void window_handle_mouse(window_t* win, int layer, int delta_x, int delta_y, int buttons);
void window_handle_key(window_t* win, int scancode);

extern window_t* g_focused_window;

#endif
