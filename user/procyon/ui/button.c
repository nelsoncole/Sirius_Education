#include "include/button.h"
#include <stdlib.h>
#include <string.h>

static void button_invalidate(component_t *c)
{
    if (!c) return;

    c->render = 1;
    c->dirty = 1;
}


int draw_button(component_t* c, window_t* win)
{
    if (!c || !win || !c->visible || !c->render)
        return 0;

    button_data_t* b = (button_data_t*)c->data;
    if (!b) return 0;

    ui_lock(&win->lock);

    draw_rect_component(
        win->buffer,
        win->width,
        win->height,
        c->x, c->y, c->w, c->h,
        c->bg,
        c->border_color,
        b->border_thickness,
        b->border_radius
    );

    if (b->label[0] != '\0') {
        int y = (c->h / 2) - (c->font_size / 2);

        draw_text_centered(
            c->font,
            b->label,
            c->x,
            c->y + y,
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


int button_event(component_t* c, event_t ev)
{
    if (!c || !c->enabled || !c->visible || !c->data)
        return 0;

    button_data_t* b = (button_data_t*)c->data;

    int inside = (
        ev.mouse.x >= c->x &&
        ev.mouse.x < c->x + c->w &&
        ev.mouse.y >= c->y &&
        ev.mouse.y < c->y + c->h
    );

    switch (ev.type) {

        case EVENT_MOUSE_MOVE:

            if (inside && !b->hovered) {
                b->hovered = 1;
                c->bg = b->bg_hover;
                button_invalidate(c);
            }
            else if (!inside && b->hovered) {
                b->hovered = 0;
                c->bg = b->bg_normal;
                button_invalidate(c);
            }

            break;

        case EVENT_MOUSE_LEFT_DOWN:

            if (inside) {
                b->pressed = 1;
                c->bg = b->bg_pressed;
                button_invalidate(c);
            }

            break;

        case EVENT_MOUSE_UP:

            if (b->pressed) {
                b->pressed = 0;

                c->bg = b->hovered
                    ? b->bg_hover
                    : b->bg_normal;

                button_invalidate(c);

                if (inside && b->on_click)
                    b->on_click(c);
            }

            break;

        default:
            break;
    }

    return c->render;
}


static void button_clear(component_t *c)
{
    if (!c || !c->data) return;

    button_data_t* b = (button_data_t*)c->data;

    b->pressed = 0;
    b->hovered = 0;

    c->bg = b->bg_normal;
}


static void button_disable(component_t *c)
{
    if (!c || !c->data) return;

    button_data_t* b = (button_data_t*)c->data;

    b->pressed = 0;
    b->hovered = 0;

    c->bg = b->bg_normal;
}


static void button_destroy(component_t *c)
{
    if (!c) return;

    free(c->data);
    c->data = NULL;
}


component_t* create_button(
    ui_context_t* ctx,
    int x, int y,
    int w, int h,
    const char* label,
    void (*on_click)(component_t *comp)
)
{
    if (!ctx || !ctx->window)
        return NULL;

    component_t* comp = calloc(1, sizeof(component_t));

    if (!comp)
        return NULL;

    button_data_t* btn = calloc(1, sizeof(button_data_t));

    if (!btn) {
        free(comp);
        return NULL;
    }

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;

    comp->type = UI_BUTTON;
    comp->visible = 1;
    comp->enabled = 1;

    comp->on_draw = draw_button;
    comp->on_event = button_event;

    comp->on_clear = button_clear;
    comp->on_disable = button_disable;
    comp->on_destroy = button_destroy;

    if (ctx->window->font)
        comp->font = *(stbtt_fontinfo*)ctx->window->font;

    comp->font_size = 18;

    comp->fg = 0xFF000000;
    comp->bg = 0xFFDDDDDD;
    comp->border_color = 0xFF222222;

    comp->render = 1;
    comp->dirty = 1;

    if (label) {
        strncpy(btn->label, label, sizeof(btn->label) - 1);
        btn->label[sizeof(btn->label) - 1] = '\0';
    }

    btn->bg_normal = 0xFFDDDDDD;
    btn->bg_hover = 0xFFEEEEEE;
    btn->bg_pressed = 0xFFAAAAAA;

    btn->border_radius = 8;
    btn->border_thickness = 1;
    btn->on_click = on_click;

    comp->data = btn;
    comp->ctx = ctx;

    if (ui_add_component(ctx, comp)) {
        if(comp->on_destroy) 
            comp->on_destroy(comp);

        free(comp);
        return NULL;
    }

    return comp;
}


void button_set_label(component_t* c, const char* new_label)
{
    if (!c || c->type != UI_BUTTON || !c->data)
        return;

    button_data_t* b = (button_data_t*)c->data;

    if (!new_label) {
        b->label[0] = '\0';
    } else {
        strncpy(b->label, new_label, sizeof(b->label) - 1);
        b->label[sizeof(b->label) - 1] = '\0';
    }

    button_invalidate(c);
}
