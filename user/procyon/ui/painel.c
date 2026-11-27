#include "include/components.h"
#include "include/painel.h"
#include <stdlib.h>
#include <string.h>

static void panel_invalidate(component_t *c)
{
    if (!c) return;

    c->render = 1;
    c->dirty = 1;

    if (c->ctx)
        ui_invalidate_region(c->ctx, c->x, c->y, c->w, c->h);
}

static void panel_on_clear(component_t *c)
{
    if (!c || c->type != UI_PANEL || !c->data) return;

    panel_invalidate(c);
}

static void panel_on_enable(component_t *c)
{
    if (!c || c->type != UI_PANEL) return;

    panel_invalidate(c);
}

static void panel_on_disable(component_t *c)
{
    if (!c || c->type != UI_PANEL) return;

    panel_invalidate(c);
}

static void panel_on_destroy(component_t *c)
{
    if (!c) return;

    panel_data_t *p = c->data;
    if (!p) return;

    free(p);
    c->data = NULL;
}

int draw_panel(component_t *c, window_t *win)
{
    if (!c || !win || !win->buffer || !c->visible ||
        !c->enabled || !c->render || !c->data)
        return 0;

    panel_data_t *p = c->data;

    ui_lock(&win->lock);

    draw_rect_component(
        win->buffer,
        win->width,
        win->height,
        c->x,
        c->y,
        c->w,
        c->h,
        p->bg,
        p->border_color,
        p->border ? p->border_thickness : 0,
        p->border_radius
    );

    c->render = 0;
    c->dirty = 0;
    c->clear = 0;

    ui_unlock(&win->lock);
    return 1;
}

component_t *create_panel(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    unsigned int bg_color,
    int border,
    unsigned int border_color)
{
    if (!ctx || !ctx->window || w <= 0 || h <= 0)
        return NULL;

    component_t *comp = calloc(1, sizeof(component_t));
    if (!comp)
        return NULL;

    panel_data_t *p = calloc(1, sizeof(panel_data_t));
    if (!p) {
        free(comp);
        return NULL;
    }

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_PANEL;
    comp->visible = 1;
    comp->enabled = 1;
    comp->on_draw = draw_panel;
    comp->on_event = NULL;
    comp->render = 1;
    comp->dirty = 1;
    comp->bg = bg_color;
    comp->data = p;
    comp->ctx = ctx;

    p->bg = bg_color;
    p->border = border ? 1 : 0;
    p->border_color = border_color;
    p->border_radius = 0;
    p->border_thickness = 1;

    comp->on_clear = panel_on_clear;
    comp->on_enable = panel_on_enable;
    comp->on_disable = panel_on_disable;
    comp->on_destroy = panel_on_destroy;

    if (ui_add_component(ctx, comp)) {
        if(comp->on_destroy) 
            comp->on_destroy(comp);

        free(comp);
        return NULL;
    }
    
    return comp;
}
