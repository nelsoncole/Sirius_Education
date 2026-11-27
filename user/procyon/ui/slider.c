#include "include/components.h"
#include "include/slider.h"
#include <stdlib.h>
#include <stdint.h>

static void slider_invalidate(component_t *c)
{
    if (!c)
        return;

    c->render = 1;
    c->dirty = 1;
}

static int slider_clamp_value(const slider_data_t *s, int value)
{
    if (!s)
        return value;

    if (value < s->min)
        return s->min;

    if (value > s->max)
        return s->max;

    return value;
}

static int slider_value_from_x(component_t *c, slider_data_t *s, int x)
{
    if (!c || !s || s->max <= s->min)
        return s ? s->min : 0;

    int radius = c->h / 2 - 1;
    if (radius < 0)
        radius = 0;

    int travel = c->w - 2 * radius;
    if (travel <= 0)
        return s->min;

    int64_t rel = (int64_t)x - c->x - radius;

    if (rel < 0)
        rel = 0;
    if (rel > travel)
        rel = travel;

    int64_t range = (int64_t)s->max - s->min;
    int64_t value = (int64_t)s->min + (rel * range) / travel;

    if (value < s->min)
        value = s->min;
    if (value > s->max)
        value = s->max;

    return (int)value;
}

static int slider_knob_x(component_t *c, slider_data_t *s)
{
    if (!c || !s || s->max <= s->min)
        return c ? c->x + c->w / 2 : 0;

    int radius = c->h / 2 - 1;
    if (radius < 0)
        radius = 0;

    int travel = c->w - 2 * radius;
    if (travel <= 0)
        return c->x + c->w / 2;

    int64_t range = (int64_t)s->max - s->min;
    int64_t offset = (int64_t)s->value - s->min;
    int64_t position = (offset * travel) / range;

    return c->x + radius + (int)position;
}

static void slider_on_clear(component_t *c)
{
    if (!c || !c->data)
        return;

    slider_data_t *s = (slider_data_t *)c->data;
    s->value = s->min;
    s->dragging = 0;
    slider_invalidate(c);
}

static void slider_on_enable(component_t *c)
{
    slider_invalidate(c);
}

static void slider_on_disable(component_t *c)
{
    if (c && c->data)
        ((slider_data_t *)c->data)->dragging = 0;

    slider_invalidate(c);
}

static void slider_on_destroy(component_t *c)
{
    if (!c)
        return;

    free(c->data);
    c->data = NULL;
}

int draw_slider(component_t *c, window_t *win)
{
    if (!c || !win || !win->buffer || !c->visible ||
        !c->enabled || !c->render || !c->data ||
        c->w <= 0 || c->h <= 0)
        return 0;

    slider_data_t *s = (slider_data_t *)c->data;
    if (s->max <= s->min)
        return 0;

    int radius = c->h / 2 - 1;
    if (radius < 0)
        radius = 0;

    int cy = c->y + c->h / 2;
    int knob_x = slider_knob_x(c, s);

    ui_lock(&win->lock);

    fill_rect(
        win->buffer, win->width, win->height,
        c->x, c->y, c->w, c->h, win->bg
    );

    draw_line(
        win->buffer, win->width, win->height,
        c->x + radius, cy,
        c->x + c->w - radius - 1, cy,
        c->border_color
    );

    if (radius > 0) {
        fill_circle(
            win->buffer, win->width, win->height,
            knob_x, cy, radius, c->fg
        );
    }

    c->render = 0;
    c->dirty = 0;
    c->clear = 0;

    ui_unlock(&win->lock);
    return 1;
}

int slider_event(component_t *c, event_t ev)
{
    if (!c || !c->data || !c->ctx || !c->ctx->window ||
        !c->visible || !c->enabled || c->type != UI_SLIDER)
        return 0;

    slider_data_t *s = (slider_data_t *)c->data;
    if (s->max <= s->min || c->w <= 0 || c->h <= 0)
        return 0;

    int changed = 0;
    int new_value = s->value;

    switch (ev.type) {
        case EVENT_MOUSE_LEFT_DOWN: {
            int radius = c->h / 2 - 1;
            if (radius < 0)
                radius = 0;

            int cy = c->y + c->h / 2;
            int knob_x = slider_knob_x(c, s);

            int64_t dx = (int64_t)ev.mouse.x - knob_x;
            int64_t dy = (int64_t)ev.mouse.y - cy;
            int64_t distance_squared = dx * dx + dy * dy;
            int64_t radius_squared = (int64_t)radius * radius;

            int inside = (int64_t)ev.mouse.x >= c->x &&
                         (int64_t)ev.mouse.x < (int64_t)c->x + c->w &&
                         (int64_t)ev.mouse.y >= c->y &&
                         (int64_t)ev.mouse.y < (int64_t)c->y + c->h;

            if (distance_squared <= radius_squared || inside) {
                s->dragging = 1;

                if (inside && distance_squared > radius_squared)
                    new_value = slider_value_from_x(c, s, ev.mouse.x);

                if (new_value != s->value) {
                    s->value = new_value;
                    changed = 1;
                }
            }
            break;
        }

        case EVENT_MOUSE_MOVE:
            if (s->dragging) {
                new_value = slider_value_from_x(c, s, ev.mouse.x);

                if (new_value != s->value) {
                    s->value = new_value;
                    changed = 1;
                }
            }
            break;

        case EVENT_MOUSE_UP:
            s->dragging = 0;
            break;

        default:
            break;
    }

    if (changed) {
        slider_invalidate(c);

        void (*on_change)(int) = s->on_change;
        int value = s->value;

        if (on_change)
            on_change(value);
    }

    return c->render;
}

component_t *create_slider(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    int min,
    int max,
    int value,
    void (*on_change)(int)
)
{
    if (!ctx || !ctx->window || w <= 0 || h <= 0 || max <= min)
        return NULL;

    component_t *comp = calloc(1, sizeof(component_t));
    if (!comp)
        return NULL;

    slider_data_t *s = calloc(1, sizeof(slider_data_t));
    if (!s) {
        free(comp);
        return NULL;
    }

    s->min = min;
    s->max = max;
    s->value = slider_clamp_value(s, value);
    s->dragging = 0;
    s->on_change = on_change;

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_SLIDER;
    comp->visible = 1;
    comp->enabled = 1;
    comp->on_draw = draw_slider;
    comp->on_event = slider_event;
    comp->on_clear = slider_on_clear;
    comp->on_enable = slider_on_enable;
    comp->on_disable = slider_on_disable;
    comp->on_destroy = slider_on_destroy;
    comp->fg = 0x222222;
    comp->border_color = 0x666666;
    comp->render = 1;
    comp->dirty = 1;
    comp->ctx = ctx;
    comp->data = s;

    if (ui_add_component(ctx, comp)) {
        if(comp->on_destroy) 
            comp->on_destroy(comp);

        free(comp);
        return NULL;
    }

    return comp;
}