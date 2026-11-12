#include "include/components.h"
#include "include/painel.h"
#include <stdlib.h>
#include <string.h>

// PANEL
int draw_panel(component_t* c, window_t* win) {
    if (!c || !c->visible || !c->render) return 0;

    panel_data_t* p = (panel_data_t*)c->data;
    if (!p) return 0;

    ui_lock(&win->lock);

    // Preencher área do painel
    draw_rect_component(win->buffer, win->width, win->height,
                    c->x, c->y, c->w, c->h,
                    p->bg,                      // cor de fundo (cinza claro)
                    p->border_color,            // borda preta
                    p->border ? p->border_thickness : 0,        // espessura borda
                    p->border_radius);          // raio do canto

    c->render--;

    ui_unlock(&win->lock);

    return 1;
}

component_t* create_panel(ui_context_t* ctx, int x, int y, int w, int h,
                          unsigned int bg_color, int border, unsigned int border_color) {
    if (ctx->count >= 64) return NULL;

    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_PANEL;
    comp->visible = 1;
    comp->on_draw = draw_panel;
    comp->on_event = 0;
    comp->render = 1;
    comp->bg = bg_color;

    panel_data_t* p = malloc(sizeof(panel_data_t));
    p->bg = bg_color;
    p->border = border;
    p->border_color = border_color;
    p->border_radius = 0;
    p->border_thickness = 1;
    comp->data = p;

    comp->ctx = ctx;
    ctx->components[ctx->count++] = comp;
    return comp;
}