#include "include/components.h"
#include "include/progressbar.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static void progressbar_invalidate(component_t *c)
{
    if (!c) return;

    c->render = 1;
    c->dirty = 1;
}

static void progressbar_on_clear(component_t *c)
{
    if (!c || c->type != UI_PROGRESSBAR || !c->data) return;

    progressbar_data_t *pb = c->data;
    pb->value = 0;

    progressbar_invalidate(c);
}

static void progressbar_on_enable(component_t *c)
{
    if (!c || c->type != UI_PROGRESSBAR) return;
    progressbar_invalidate(c);
}

static void progressbar_on_disable(component_t *c)
{
    if (!c || c->type != UI_PROGRESSBAR) return;
    progressbar_invalidate(c);
}

static void progressbar_on_destroy(component_t *c)
{
    if (!c) return;

    progressbar_data_t *pb = c->data;
    if (!pb) return;

    free(pb);
    c->data = NULL;
}

int draw_progressbar(component_t *c, window_t *win)
{
    if (!c || !win || !win->buffer || !c->visible ||
        !c->enabled || !c->render || !c->data)
        return 0;

    progressbar_data_t *pb = c->data;

    ui_lock(&win->lock);

    draw_rect_component(
        win->buffer,
        win->width,
        win->height,
        c->x,
        c->y,
        c->w,
        c->h,
        c->bg,
        c->border_color,
        1,
        4
    );

    int filled = 0;

    if (pb->max > 0 && c->w > 0 && c->h > 0 && pb->value > 0) {
        int value = pb->value;
        if (value > pb->max)
            value = pb->max;

        filled = (int)(((int64_t)value * c->w) / pb->max);

        if (filled > c->w)
            filled = c->w;

        if (filled > 0) {
            draw_rect_component(
                win->buffer,
                win->width,
                win->height,
                c->x,
                c->y,
                filled,
                c->h,
                pb->color,
                pb->color,
                0,
                4
            );
        }
    }

    c->render = 0;
    c->dirty = 0;
    c->clear = 0;

    ui_unlock(&win->lock);
    return 1;
}

void progressbar_set_value(component_t *c, int v)
{
    if (!c || c->type != UI_PROGRESSBAR || !c->data)
        return;

    progressbar_data_t *pb = c->data;

    if (pb->max <= 0)
        v = 0;
    else {
        if (v < 0) v = 0;
        if (v > pb->max) v = pb->max;
    }

    if (pb->value == v)
        return;

    pb->value = v;
    progressbar_invalidate(c);
}

component_t *create_progressbar(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    int max)
{
    if (!ctx || !ctx->window || w <= 0 || h <= 0)
        return NULL;

    component_t *comp = calloc(1, sizeof(component_t));
    if (!comp)
        return NULL;

    progressbar_data_t *pb = calloc(1, sizeof(progressbar_data_t));
    if (!pb) {
        free(comp);
        return NULL;
    }

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_PROGRESSBAR;
    comp->visible = 1;
    comp->enabled = 1;
    comp->on_draw = draw_progressbar;
    comp->on_event = NULL;
    comp->bg = 0xFFDDDDDD;
    comp->border_color = 0xFF444444;
    comp->render = 1;
    comp->dirty = 1;
    comp->data = pb;
    comp->ctx = ctx;

    pb->value = 0;
    pb->max = max > 0 ? max : 0;
    pb->color = 0xFF00AA00;

    comp->on_clear = progressbar_on_clear;
    comp->on_enable = progressbar_on_enable;
    comp->on_disable = progressbar_on_disable;
    comp->on_destroy = progressbar_on_destroy;

    if (ui_add_component(ctx, comp)) {
        if(comp->on_destroy) 
            comp->on_destroy(comp);

        free(comp);
        return NULL;
    }
    
    return comp;
}
