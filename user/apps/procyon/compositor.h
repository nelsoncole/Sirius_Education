#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include "window.h"

void compositor_init();
void compositor_add_window(window_t* win, int layer);
void compositor_remove_window(window_t* win);
void compositor_add_window_child(window_t* win, window_t* parent);
void compositor_remove_window_child(window_t* win);
void compositor_render();
void compositor_draw_window(window_t* win);
void compositor_draw_cursor();

// Handler
void compositor_handle_key_down(int key, int type);
void compositor_remove_invalid_window();


#endif