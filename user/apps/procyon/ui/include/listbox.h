#ifndef LISTBOX_H
#define LISTBOX_H

typedef struct {
    char** items;       //itens
    int count;
    int selected;
    int expanded;         // 1 = dropdown 
    int focused;
    int item_height;                   // altura de cada item (px)
    int scroll_offset;
    // Scrollbar estado
    int dragging_scroll;   // 1 = arrastando thumb
    int drag_offset_y;     // posição relativa do clique no thumb
    volatile int spinlock;
    void (*on_select)(int index, const char* item);
} listbox_data_t;


void listbox_set_items(component_t* c, const char* items[], int count);

component_t* create_listbox(ui_context_t* ctx, int x, int y, int w, int h,
                            const char* items[], int count, void (*on_select)(int, const char*));

#endif