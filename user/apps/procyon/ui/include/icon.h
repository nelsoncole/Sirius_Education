#ifndef ICON_H
#define ICON_H

#include "components.h"

typedef struct {
    int width, height;          // Tamanho do ícone
    unsigned char* image;       // Dados do ícone (RGBA)
    unsigned color;             // Cor do icon
} ui_icon_t;

#endif