#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include "components.h"

// Dados da ProgressBar
typedef struct {
    int value;     // valor atual
    int max;       // valor máximo
    unsigned int color; // cor da barra preenchida
} progressbar_data_t;

void progressbar_set_value(component_t* c, int v);

component_t* create_progressbar(ui_context_t* ctx, int x, int y, int w, int h, int max);

#endif