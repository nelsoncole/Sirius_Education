#ifndef SLIDER_H
#define SLIDER_H

#include "components.h"

typedef struct {
    int min, max;
    int value;
    int dragging;
    volatile int spinlock;
    void (*on_change)(int value);
} slider_data_t;

component_t* create_slider(ui_context_t* ctx, int x, int y, int w, int h,
                           int min, int max, int value, void (*on_change)(int));
#endif