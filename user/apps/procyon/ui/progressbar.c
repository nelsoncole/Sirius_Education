#include "include/components.h"
#include "include/progressbar.h"
#include <stdlib.h>
#include <string.h>

// PROGRESSBAR
int draw_progressbar(component_t* c, window_t* win) {
    if (!c || !c->visible || !c->render) return 0;

    progressbar_data_t* pb = (progressbar_data_t*)c->data;

    ui_lock(&win->lock);

    // Fundo
    draw_rect_component(win->buffer, win->width, win->height,
                        c->x, c->y, c->w, c->h,
                        c->bg, c->border_color, 1, 4);

    // Barra preenchida
    int filled = (pb->value * c->w) / pb->max;
    if (filled > 0) {
        draw_rect_component(win->buffer, win->width, win->height,
                            c->x, c->y, filled, c->h,
                            pb->color, pb->color, 0, 4);
    }

    c->render--;

    ui_unlock(&win->lock);

    return 1;
}

void progressbar_set_value(component_t* c, int v) {
    progressbar_data_t* pb = (progressbar_data_t*)c->data;
    if (!pb) return;

    if (v < 0) v = 0;
    if (v > pb->max) v = pb->max;
    pb->value = v;
    c->render = 1;
    c->ctx->window->invalid = 1;
}

component_t* create_progressbar(ui_context_t* ctx, int x, int y, int w, int h, int max) {
    if (ctx->count >= 64) return NULL;

    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_PROGRESSBAR;
    comp->visible = 1;
    comp->on_draw = draw_progressbar;
    comp->on_event = NULL;
    comp->bg = 0xFFDDDDDD;
    comp->border_color = 0x444444;
    comp->render = 1;

    progressbar_data_t* pb = malloc(sizeof(progressbar_data_t));
    pb->value = 0;
    pb->max = max;
    pb->color = 0xFF00AA00; // verde
    comp->data = pb;

    comp->ctx = ctx;
    ctx->components[ctx->count++] = comp;
    return comp;
}