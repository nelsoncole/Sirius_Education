#ifndef LAYER_H
#define LAYER_H

#include "window.h"

#define LAYER_COUNT 3               // Ex: 0 = fundo, 1 = janelas normais, 2 = sobreposição
#define MAX_WINDOWS_PER_LAYER 32    // Máximo de janelas por camada

extern window_t* layers[LAYER_COUNT][MAX_WINDOWS_PER_LAYER];
extern int layer_counts[LAYER_COUNT];

void move_window_to_top(int layer_index, window_t* win);

#endif