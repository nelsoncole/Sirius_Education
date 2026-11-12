#ifndef ICONBUTTON_H
#define ICONBUTTON_H

#include "components.h"
#include "icon.h"

typedef struct {
    char label[256];          // Texto do botão
    ui_icon_t icon;           // Ícone associado
    int pressed;              // Estado pressionado
    int hovered;              // Estado hover

    // Configuração visual
    int transparent_bg;       // 1 = fundo transparente, 0 = cor sólida
    unsigned int bg_normal;   // Cor de fundo normal
    unsigned int bg_hover;    // Cor de fundo quando hover
    unsigned int bg_pressed;  // Cor de fundo quando clicado
    int border_radius;        // Arredondamento das bordas
    int border_thickness;     // Espessura da borda

    // Controle do ícone
    int icon_size;            // Tamanho do ícone
    int show_icon;            // 1 = mostra ícone, 0 = não mostra
    int show_label;           // 1 = mostra texto, 0 = só ícone
    int spacing;              // Espaço entre ícone e label

    volatile int spinlock;
    // Callback
    void (*on_click)();       // Função chamada no clique
} iconbutton_data_t;

component_t* create_iconbutton(ui_context_t* ctx, int x, int y, int w, int h,
    const char* label, const char* icon_path, int icon_size, void (*on_click)());

#endif