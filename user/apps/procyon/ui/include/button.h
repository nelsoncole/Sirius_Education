#ifndef BUTTON_H
#define BUTTON_H

#include "components.h"

typedef struct {
    char label[256];
    int pressed;
    int hovered;
    
     // Configuração visual
    int transparent_bg;       // 1 = fundo transparente, 0 = cor sólida
    unsigned int bg_normal;   // Cor de fundo normal
    unsigned int bg_hover;    // Cor de fundo quando hover
    unsigned int bg_pressed;  // Cor de fundo quando clicado
    int border_radius;        // Arredondamento das bordas
    int border_thickness;     // Espessura da borda
    volatile int spinlock;
    void (*on_click)();
} button_data_t;


component_t* create_button(ui_context_t* ctx, int x, int y, int w, int h,
    const char* label, void (*on_click)());
    
void button_set_label(component_t* c, const char* new_label);

#endif