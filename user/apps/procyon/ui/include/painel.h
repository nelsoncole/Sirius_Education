#ifndef PAINEL_H
#define PAINEL_H

#include "components.h"

typedef struct {
    unsigned int bg;
    int border;              // 0 = sem borda, 1 = com borda
    unsigned int border_color;
    int border_radius;        // Arredondamento das bordas
    int border_thickness;     // Espessura da borda
} panel_data_t;

component_t* create_panel(ui_context_t* ctx, int x, int y, int w, int h,
                          unsigned int bg_color, int border, unsigned int border_color);

#endif