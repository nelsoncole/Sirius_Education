#include "include/components.h"
#include "include/radiobutton.h"
#include <stdlib.h>
#include <string.h>

static void radiobutton_invalidate(component_t *c)
{
    if (!c) return;

    c->render = 1;
    c->dirty = 1;
}

static int radiobutton_contains(const component_t *c, int x, int y)
{
    if (!c) return 0;

    return x >= c->x && x < c->x + c->w &&
           y >= c->y && y < c->y + c->h;
}

static void radiobutton_on_clear(component_t *c)
{
    if (!c || !c->data) return;

    radiobutton_data_t *rb = (radiobutton_data_t *)c->data;
    rb->selected = 0;
    radiobutton_invalidate(c);
}

static void radiobutton_on_enable(component_t *c)
{
    radiobutton_invalidate(c);
}

static void radiobutton_on_disable(component_t *c)
{
    radiobutton_invalidate(c);
}

static void radiobutton_on_destroy(component_t *c)
{
    if (!c) return;

    free(c->data);
    c->data = NULL;
}

int draw_radiobutton(component_t *c, window_t *win)
{
    if (!c || !win || !win->buffer || !c->visible ||
        !c->enabled || !c->render || !c->data)
        return 0;

    radiobutton_data_t *rb = (radiobutton_data_t *)c->data;
    if (c->w <= 0 || c->h <= 0) return 0;

    ui_lock(&win->lock);

    draw_rect_component(
        win->buffer, win->width, win->height,
        c->x, c->y, c->h, c->h,
        c->bg, c->border_color, 1, c->h / 2
    );

    if (rb->selected) {
        int radius = c->h / 2 - 4;
        if (radius > 0) {
            fill_circle(
                win->buffer, win->width, win->height,
                c->x + c->h / 2, c->y + c->h / 2,
                radius, c->fg
            );
        }
    }

    int text_x = c->x + c->h + 6;
    int text_width = c->w - c->h - 6;

    if (text_width > 0) {
        fill_rect(
            win->buffer, win->width, win->height,
            text_x, c->y, text_width, c->h, win->bg
        );

        if (c->font_size > 0) {
            int text_y = c->y + ((c->h - c->font_size) / 2);
            draw_text(
                c->font, rb->label, text_x, text_y,
                text_width, c->fg, c->font_size, win
            );
        }
    }

    c->render = 0;
    c->dirty = 0;
    c->clear = 0;

    ui_unlock(&win->lock);
    return 1;
}

int radiobutton_event(component_t *c, event_t ev)
{
    if (!c || !c->ctx || !c->ctx->window || !c->data ||
        !c->enabled || !c->visible)
        return 0;

    if (ev.type != EVENT_MOUSE_LEFT_DOWN)
        return c->render;

    if (!radiobutton_contains(c, ev.mouse.x, ev.mouse.y))
        return c->render;

    radiobutton_data_t *rb = (radiobutton_data_t *)c->data;
    if (rb->selected)
        return c->render;

    ui_context_t *ctx = c->ctx;

    for (int i = 0; i < ctx->count; i++) {
        component_t *other = ctx->components[i];

        if (!other || other->type != UI_RADIOBUTTON || !other->data)
            continue;

        radiobutton_data_t *other_rb =
            (radiobutton_data_t *)other->data;

        if (other_rb->group_id != rb->group_id)
            continue;

        if (other_rb->selected) {
            other_rb->selected = 0;
            radiobutton_invalidate(other);
        }
    }

    rb->selected = 1;
    radiobutton_invalidate(c);

    void (*on_select)(int) = rb->on_select;
    int group_id = rb->group_id;

    if (on_select)
        on_select(group_id);

    return 1;
}

component_t *create_radiobutton(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    const char *label,
    int group,
    int selected,
    void (*on_select)(int)
)
{
    if (!ctx || !ctx->window || !ctx->window->font ||
        w <= 0 || h <= 0)
        return NULL;

    component_t *comp = calloc(1, sizeof(component_t));
    if (!comp)
        return NULL;

    radiobutton_data_t *rb = calloc(1, sizeof(radiobutton_data_t));
    if (!rb) {
        free(comp);
        return NULL;
    }

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_RADIOBUTTON;
    comp->visible = 1;
    comp->enabled = 1;

    comp->on_draw = draw_radiobutton;
    comp->on_event = radiobutton_event;
    comp->on_clear = radiobutton_on_clear;
    comp->on_enable = radiobutton_on_enable;
    comp->on_disable = radiobutton_on_disable;
    comp->on_destroy = radiobutton_on_destroy;

    comp->font = *(stbtt_fontinfo *)ctx->window->font;
    comp->font_size = 16;
    comp->fg = 0x000000;
    comp->bg = 0xFFFFFFFF;
    comp->border_color = 0x444444;
    comp->render = 1;
    comp->dirty = 1;
    comp->ctx = ctx;
    comp->data = rb;

    if (label) {
        strncpy(rb->label, label, sizeof(rb->label) - 1);
        rb->label[sizeof(rb->label) - 1] = '\0';
    }

    rb->selected = selected ? 1 : 0;
    rb->group_id = group;
    rb->on_select = on_select;

    if (ui_add_component(ctx, comp)) {
        if(comp->on_destroy) 
            comp->on_destroy(comp);

        free(comp);
        return NULL;
    }

    return comp;
}