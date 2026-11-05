#include "layer.h"
#include "window.h"

window_t* layers[LAYER_COUNT][MAX_WINDOWS_PER_LAYER];
int layer_counts[LAYER_COUNT] = {0};

void move_window_to_top(int layer_index, window_t* win) {
    if (layer_index < 0 || layer_index >= LAYER_COUNT) return;

    int count = layer_counts[layer_index];
    int index = -1;

    // Procurar a janela na camada
    for (int i = 0; i < count; i++) {
        if (layers[layer_index][i] == win) {
            index = i;
            break;
        }
    }

    if (index == -1) return; // Janela não encontrada

    // Remover janela deslocando as outras para a esquerda
    for (int i = index; i < count - 1; i++) {
        layers[layer_index][i] = layers[layer_index][i + 1];
    }

    // Colocar a janela no final
    layers[layer_index][count - 1] = win;
}
