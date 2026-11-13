#ifndef LABEL_H
#define LABEL_H

#include "components.h"

typedef struct {
    char text[256];
} label_data_t;

component_t* create_label(ui_context_t* ctx, int x, int y, int w, const char* text, unsigned int color);

void label_set_text(component_t* c, const char* new_text);

#endif