#ifndef GRIDVIEW_H
#define GRIDVIEW_H

#include "components.h"

// ------------------------------
// Estrutura do GridView
// ------------------------------
typedef struct {
    char*** columns;       // cada linha é um array de strings (colunas)
    int row_count;         // número total de linhas (incluindo cabeçalho)
    int col_count;         // número de colunas
    int selected;          // linha selecionada (-1 = nenhuma)
    int focused;           // 1 = foco
    int row_height;        // altura da linha em px
    int scroll_offset;     // linhas scrolladas (ignora cabeçalho)
    int dragging_scroll;
    int drag_offset_y;
    volatile int spinlock;
    void (*on_select)(int index, char** row);
} gridview_data_t;

void gridview_set_items(component_t* c, char*** new_data, int rows, int cols);
component_t* create_gridview(ui_context_t* ctx, int x, int y, int w, int h,
                             char*** data, int rows, int cols,
                             void (*on_select)(int, char**));

#endif