#ifndef COMBOBOX_H
#define COMBOBOX_H

#include "components.h"

typedef struct {
    char** items;
    int count;
    int selected;
    int expanded;
    int scroll_offset;       // <- para scroll
    int item_height;         // <- altura por item (igual listbox)
    int focused;
    int dragging_scroll;
    int drag_offset_y;
    window_t* win;
    volatile int spinlock;
    void (*on_select)(int index, const char* item);
} combobox_data_t;

component_t* create_combobox(ui_context_t* ctx, int x, int y, int w, int h,
                             const char* items[], int count,
                             void (*on_select)(int, const char*));

#endif