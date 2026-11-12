#ifndef CHECKBOX_H
#define CHECKBOX_H

#include "components.h"

// Dados do Checkbox
typedef struct {
    int checked;                  // 0 = desmarcado, 1 = marcado
    char label[128];              // texto exibido ao lado
    volatile int spinlock;
    void (*on_change)(int state); // callback quando mudar (recebe 0/1)
} checkbox_data_t;

component_t* create_checkbox(ui_context_t* ctx, int x, int y, int w, int h, const char* label, int initial, void (*on_change)(int));


#endif