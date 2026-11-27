#include "include/label.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void label_invalidate(component_t *c)
{
    if (!c) return;

    c->render = 1;
    c->dirty = 1;

    if (c->ctx)
        ui_invalidate_region(c->ctx, c->x, c->y, c->w, c->h);
}

static void label_on_clear(component_t *c)
{
    if (!c || c->type != UI_LABEL || !c->data) return;

    label_data_t *l = c->data;
    l->text[0] = '\0';
    label_invalidate(c);
}

static void label_on_enable(component_t *c)
{
    if (!c || c->type != UI_LABEL) return;
    label_invalidate(c);
}

static void label_on_disable(component_t *c)
{
    if (!c || c->type != UI_LABEL) return;
    label_invalidate(c);
}

static void label_on_destroy(component_t *c)
{
    if (!c) return;

    label_data_t *l = c->data;
    if (!l) return;

    free(l);
    c->data = NULL;
}

int draw_label(component_t *c, window_t *win)
{
    if (!c || !win || !win->buffer || !c->visible ||
        !c->enabled || !c->render || !c->data)
        return 0;

    label_data_t *l = c->data;

    ui_lock(&win->lock);

    fill_rect(
        win->buffer,
        win->width,
        win->height,
        c->x,
        c->y,
        c->w,
        c->h,
        c->bg
    );

    if (l->text[0]) {
        draw_text(
            c->font,
            l->text,
            c->x,
            c->y,
            c->w,
            c->fg,
            c->font_size,
            win
        );
    }

    c->render = 0;
    c->dirty = 0;
    c->clear = 0;

    ui_unlock(&win->lock);
    return 1;
}

void label_set_text(component_t *c, const char *new_text)
{
    if (!c || c->type != UI_LABEL || !c->data)
        return;

    label_data_t *l = c->data;

    if (!new_text)
        new_text = "";

    strncpy(l->text, new_text, sizeof(l->text) - 1);
    l->text[sizeof(l->text) - 1] = '\0';

    label_invalidate(c);
}

component_t *create_label(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    const char *text,
    unsigned int color)
{
    if (!ctx || !ctx->window || !ctx->window->font || w <= 0)
        return NULL;

    component_t *comp = calloc(1, sizeof(component_t));
    if (!comp)
        return NULL;

    label_data_t *lbl = calloc(1, sizeof(label_data_t));
    if (!lbl) {
        free(comp);
        return NULL;
    }

    comp->x = ctx->window->client.x + x;
    comp->y = ctx->window->client.y + y;
    comp->w = w;
    comp->h = 16;
    comp->type = UI_LABEL;
    comp->visible = 1;
    comp->enabled = 1;
    comp->on_draw = draw_label;
    comp->on_event = NULL;
    comp->font = *(stbtt_fontinfo *)ctx->window->font;
    comp->font_size = 16;
    comp->fg = color;
    comp->bg = ctx->window->bg;
    comp->render = 1;
    comp->dirty = 1;
    comp->ctx = ctx;
    comp->data = lbl;

    if (text) {
        strncpy(lbl->text, text, sizeof(lbl->text) - 1);
        lbl->text[sizeof(lbl->text) - 1] = '\0';
    }

    comp->on_clear = label_on_clear;
    comp->on_enable = label_on_enable;
    comp->on_disable = label_on_disable;
    comp->on_destroy = label_on_destroy;

    if (ui_add_component(ctx, comp)) {
        if(comp->on_destroy) 
            comp->on_destroy(comp);

        free(comp);
        return NULL;
    }
    
    return comp;
}
