#ifndef EDITBOX_H
#define EDITBOX_H

#include "components.h"

typedef struct {
    char text[4096];
    int cursor_pos;
    int focused;
    int border_radius;        // Arredondamento das bordas
    int border_thickness;     // Espessura da borda

    int multiline;            // 0 = uma linha, 1 = várias linhas
    int scroll_x;             // scroll horizontal em px
    int scroll_y;             // scroll vertical em linhas
    int hide_input;           // 1 = mostrar '*' em vez de texto
    int cursor_line;          // linha onde o cursor está

    // controle de cursor piscante
    unsigned long last_toggle;
    int cursor_visible;
    volatile int spinlock;
} editbox_data_t;

component_t* create_editbox(ui_context_t* ctx, int x, int y, int w, int h);
void editbox_set_text(component_t* c, const char* text);
void editbox_set_text_ex(component_t* c, const char* text, int cursor_pos);

#endif