#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "../../procyon_ipc.h"
#include "../../window.h"
#include "../../text.h"
#include "../../event.h"
#include "../../image.h"
#include "../../draw_rect.h"
#include "../../draw_line.h"

typedef enum {
    UI_NONE,
    UI_BUTTON,
    UI_EDITTEXT,
    UI_LABEL,
    UI_CHECKBOX,
    UI_PROGRESSBAR,
    UI_RADIOBUTTON,
    UI_SLIDER,
    UI_LISTBOX,
    UI_COMBOBOX,
    UI_CONTEXTMENU,
    UI_CONTEXTSUBMENU,
    UI_PANEL,
    UI_FILE_LISTBOX,
    UI_TERMINAL,
    UI_VIEWPORT
} component_type_t;

typedef struct component {
    int x, y, w, h;       // posição relativa à janela
    component_type_t type;
    int visible;
    stbtt_fontinfo font;
    font_cache_t font_cache;
    int font_size;
    unsigned int fg, bg, border_color;

    int render;

    int (*on_draw)(struct component*, window_t* win);
    void (*on_event)(struct component*, event_t ev);
    void* data; // ponteiro para dados específicos (ex: texto do botão)
    int clear;

    struct ui_context* ctx; 
} component_t;

typedef struct ui_context {
    int fd;
    window_t* window;
    component_t* components[64]; // até 64 widgets por janela
    int count;
    int modal_active;   // 0 = normal, 1 = algum menu/combobox aberto
    component_t* modal_owner; // quem está ativo (menu/combobox)
    component_t* sub_modal_owner; // sub componente quem está ativo (menu/combobox)
} ui_context_t;


static inline void ui_lock(volatile int *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        while (*lock); // espera ativa (menos agressivo)
    }
}

static inline void ui_unlock(volatile int *lock) {
    __sync_lock_release(lock);
}

void ui_draw(ui_context_t* ctx);
void ui_event(ui_context_t* ctx, event_t ev);

#endif