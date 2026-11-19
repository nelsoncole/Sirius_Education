#ifndef CONSOLE_H
#define CONSOLE_H

#include "components.h"

#define CONSOLE_SCROLLBAR_WIDTH 8

typedef struct {
    ui_context_t *ctx;
    window_t *window;

    int content_height;
    int scroll_offset;

    int cursor_x;
    int cursor_y;

    int char_width;
    int char_height;

    int scrollbar_width;
    int dragging_scroll;
    int drag_offset_y;

    int border_radius;
    int border_thickness;
    
    int width;
    int height;

    int background_render;

} console_data_t;

component_t *create_console(ui_context_t *ctx, int x, int y, int w, int h);

void console_set_content_height(component_t *console, int content_height);
void console_set_offset(component_t *console, int offset);

int draw_console(component_t *c, window_t *win);
int console_event(component_t *c, event_t ev);

int console_put_char(component_t *console, char ch);
int console_write(component_t *console, const char *text);
int console_newline(component_t *console);

#endif