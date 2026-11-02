
#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include <stddef.h>

#include "event.h"

#define WINDOW_TITLE_MAX 256

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

    event_t event_queue[MAX_EVENTS];
    int event_head;
    int event_tail;

    int last_mouse_x;
    int last_mouse_y;

    int invalid;

    int lock;

    unsigned long pid;

    struct window_t *parent;

}__attribute__((packed)) window_t;

window_t* window_create(int x, int y, int width, int height, char *title, unsigned int bg_color, unsigned int fg_color);
void window_destroy(window_t* win);

#endif
