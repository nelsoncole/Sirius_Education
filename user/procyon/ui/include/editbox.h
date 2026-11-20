#ifndef EDITBOX_H
#define EDITBOX_H

#include "components.h"

typedef enum {
    EDITBOX_TEXT,
    EDITBOX_PASSWORD,
    EDITBOX_NUMBER
} editbox_type_t;

typedef enum {
    EDITBOX_ALIGN_LEFT,
    EDITBOX_ALIGN_CENTER,
    EDITBOX_ALIGN_RIGHT
} editbox_horizontal_align_t;

typedef enum {
    EDITBOX_ALIGN_TOP,
    EDITBOX_ALIGN_VCENTER,
    EDITBOX_ALIGN_BOTTOM
} editbox_vertical_align_t;

typedef struct {
    ui_context_t *ctx;

    editbox_type_t input_type;
    editbox_horizontal_align_t horizontal_align;
    editbox_vertical_align_t vertical_align;
    int font_size;

    char *text;
    size_t text_length;
    size_t text_capacity;

    int cursor_pos;
    int cursor_x;
    int cursor_y;

    int focused;
    int multiline;
    int hide_input;

    int vertical_scroll;
    int horizontal_scroll;

    int scroll_offset_y;
    int scroll_offset_x;

    int content_width;
    int content_height;

    int scrollbar_width;
    int show_vertical_scrollbar;
    int show_horizontal_scrollbar;
    unsigned int scrollbar_bg;
    unsigned int scrollbar_fg;

    int dragging_vertical;
    int dragging_horizontal;

    int drag_offset_x;
    int drag_offset_y;

    int border_radius;
    int border_thickness;
    int background_render;

    int char_width;
    int char_height;

    volatile int spinlock;
} editbox_data_t;

component_t *create_editbox(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    int font_size,
    editbox_type_t type
);

void editbox_set_text(component_t *c, const char *text);
void editbox_set_multiline(component_t *c, int enabled);

void editbox_set_alignment(
    component_t *c,
    editbox_horizontal_align_t horizontal,
    editbox_vertical_align_t vertical
);

void editbox_set_vertical_alignment(
    component_t *c,
    editbox_vertical_align_t vertical
);

void editbox_set_scrollbars(component_t *c, int vertical, int horizontal);
void editbox_set_offset(component_t *c, int offset_x, int offset_y);
int editbox_write(component_t *c, const char *text);
int draw_editbox(component_t *c, window_t *win);
int editbox_event(component_t *c, event_t ev);

/*

Exemplo do uso do alinhamento

    // Texto no canto superior esquerdo
    editbox_set_alignment(
        name,
        EDITBOX_ALIGN_LEFT,
        EDITBOX_ALIGN_TOP
    );

    // Texto centralizado horizontal e verticalmente
    editbox_set_alignment(
        name,
        EDITBOX_ALIGN_CENTER,
        EDITBOX_ALIGN_VCENTER
    );

    // Texto alinhado à direita e ao fundo
    editbox_set_alignment(
        name,
        EDITBOX_ALIGN_RIGHT,
        EDITBOX_ALIGN_BOTTOM
    );


    // Alinhado ao topo
    editbox_set_vertical_alignment(name, EDITBOX_ALIGN_TOP);

    // Centralizado verticalmente
    editbox_set_vertical_alignment(name, EDITBOX_ALIGN_VCENTER);

    // Alinhado ao fundo
    editbox_set_vertical_alignment(name, EDITBOX_ALIGN_BOTTOM);

*/

#endif
