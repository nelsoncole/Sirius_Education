#ifndef TEXTBOX_H
#define TEXTBOX_H

#include "editbox.h"

typedef struct textbox_line {
    size_t start;
    size_t length;
    int width;
} textbox_line_t;

typedef struct textbox_data {
    ui_context_t *ctx;
    volatile int spinlock;

    char *text;
    size_t text_length;

    textbox_line_t *lines;
    size_t line_count;
    size_t line_capacity;

    int font_size;
    int char_height;

    int content_width;
    int content_height;

    int scroll_offset_x;
    int scroll_offset_y;

    int scrollbar_width;
    int border_thickness;
    int border_radius;

    int show_vertical_scrollbar;
    int show_horizontal_scrollbar;
    int word_wrap;

    int dragging_vertical;
    int dragging_horizontal;
    int drag_offset_x;
    int drag_offset_y;

    int background_render;

    uint32_t scrollbar_bg;
    uint32_t scrollbar_fg;

    editbox_horizontal_align_t horizontal_align;
    editbox_vertical_align_t vertical_align;
} textbox_data_t;

component_t *create_textbox(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    int font_size
);

void textbox_set_text(component_t *c, const char *text);
void textbox_set_alignment(
    component_t *c,
    editbox_horizontal_align_t horizontal,
    editbox_vertical_align_t vertical
);
void textbox_set_scrollbars(component_t *c, int vertical, int horizontal);
void textbox_set_word_wrap(component_t *c, int enabled);
void textbox_set_offset(component_t *c, int offset_x, int offset_y);
int draw_textbox(component_t *c, window_t *win);
int textbox_event(component_t *c, event_t ev);

#endif
