#ifndef RADIOBUTTON_H
#define RADIOBUTTON_H

#include "components.h"

typedef struct {
    int selected;                    // 1 se marcado, 0 se não
    char label[128];                 // texto ao lado
    int group_id;                    // ID do grupo
    volatile int spinlock;
    void (*on_select)(int group_id); // callback quando selecionado
} radiobutton_data_t;

component_t* create_radiobutton(ui_context_t* ctx, int x, int y, int w, int h,
                                const char* label, int group, int selected,
                                void (*on_select)(int));

#endif
