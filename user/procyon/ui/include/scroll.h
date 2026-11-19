
#ifndef SCROLL_H
#define SCROLL_H

#include "components.h"

#define SCROLLBAR_WIDTH 8

typedef enum
{
    SCROLL_LAYOUT_VERTICAL,
    SCROLL_LAYOUT_HORIZONTAL,
    SCROLL_LAYOUT_GRID
} scroll_layout_t;

typedef struct {
    component_t **children;
    int child_count;
    scroll_layout_t layout;
    int spacing_x;
    int spacing_y;
    int item_width;
    int item_height;
    int child_capacity;
    int content_height;
    int scroll_offset;
    int dragging_scroll;
    int drag_offset_y;
    int focused;
    int scrollbar_width;
    int background_render;
    int border_radius;        // Arredondamento das bordas
    int border_thickness;     // Espessura da borda
    int width;
    int height;
    ui_context_t *ctx;
} scroll_data_t;

component_t *create_scroll(ui_context_t *ctx, int x, int y, int w, int h);

int scroll_add_component(component_t *scroll, component_t *child);

void scroll_set_content_height(component_t *scroll, int content_height);

void scroll_set_offset(component_t *scroll, int offset);

int draw_scroll(component_t *c, window_t *win);

int scroll_event(component_t *c, event_t ev);

void scroll_set_layout(
    component_t *scroll,
    scroll_layout_t layout,
    int spacing_x,
    int spacing_y,
    int item_width,
    int item_height
);

void scroll_recalculate_layout(component_t *scroll);

#endif
