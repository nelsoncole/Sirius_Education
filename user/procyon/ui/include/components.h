#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "../../procyon_ipc.h"
#include "../../window.h"
#include "../../text.h"
#include "../../event.h"
#include "../../image.h"
#include "../../draw_rect.h"
#include "../../draw_line.h"
#include "../../color.h"
#include "../../keymap.h"

typedef enum {
    UI_NONE,
    UI_BUTTON,
    UI_EDITTEXT,
    UI_TEXTBOX,
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
    UI_SCROLL,
    UI_CONSOLE,
    UI_FILE_LISTBOX,
    UI_VIEWPORT,
} component_type_t;

typedef struct component {
    int id;
    char command[256];

    int x, y, w, h;
    component_type_t type;

    int visible;
    int enabled;

    stbtt_fontinfo font;
    font_cache_t font_cache;
    int font_size;

    unsigned int fg, bg, border_color;

    int render;
    int dirty;
    int clear;

    /* Ciclo de desenho e eventos */
    int (*on_draw)(struct component*, window_t*);
    int (*on_event)(struct component*, event_t);

    /* Ciclo de vida */
    void (*on_clear)(struct component*);
    void (*on_destroy)(struct component*);
    void (*on_enable)(struct component*);
    void (*on_disable)(struct component*);

    unsigned char *free_font_buffer;

    struct ui_context* ctx;

    void* data;

} component_t;

#define UI_MAX_DAMAGE_REGIONS 128

typedef struct {
    int x;
    int y;
    int w;
    int h;
} ui_damage_t;

typedef struct ui_context {
    int fd;
    window_t* window;

    component_t** components;
    int count;

    int modal_active;
    component_t* modal_owner;
    component_t* sub_modal_owner;

    ui_damage_t damage[UI_MAX_DAMAGE_REGIONS];
    int damage_count;
    int full_redraw;

    int suppress_server_notify;

    volatile int lock;
} ui_context_t;


/* Sincronização */

static inline void ui_lock(volatile int *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        while (*lock);
    }
}

static inline void ui_unlock(volatile int *lock) {
    __sync_lock_release(lock);
}


/* Gestão de componentes */

int ui_add_component(ui_context_t *ctx, component_t *component);
int ui_remove_component(ui_context_t *ctx, component_t *component);

/* Ciclo de vida */

void ui_clear_component(component_t *component);
void ui_enable_component(component_t *component);
void ui_disable_component(component_t *component);
void ui_destroy_component(component_t *component);
void ui_destroy_all_components(ui_context_t *ctx);

/* Interface */

void ui_draw(ui_context_t* ctx);
void ui_event(ui_context_t* ctx, event_t ev);

void ui_invalidate_region(ui_context_t *ctx, int x, int y, int w, int h);

int ui_get_damage( ui_context_t *ctx, ui_damage_t *regions, 
    int capacity, int *full_redraw );

void ui_hide_component(component_t *component);
void ui_show_component(component_t *component);

#endif
