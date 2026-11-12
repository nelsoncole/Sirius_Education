#include "include/components.h"
#include "include/slider.h"
#include <stdlib.h>
#include <string.h>

// SLIDER
int draw_slider(component_t* c, window_t* win) {
    if (!c || !c->visible || !c->render) return 0;

    slider_data_t* s = (slider_data_t*)c->data;

    unsigned int* buffer = win->buffer;

    ui_lock(&win->lock);

    // Apagar área antiga (background da janela na área do slider)
    
    fill_rect(buffer, win->width, win->height, c->x, c->y, c->w, c->h, win->bg);

    // Linha base (no meio vertical)
    int cy = c->y + c->h / 2;
    draw_line(buffer, win->width, win->height, c->x, cy, c->x + c->w, cy, c->border_color);

    // Raio do knob (um pouco menor que metade da altura, para caber bem)
    int radius = (c->h / 2) - 1;

    // Posição do knob (ajustada para o raio)
    int knob_x = c->x + ((s->value - s->min) * (c->w - 2 * radius)) / (s->max - s->min) + radius;

    // Knob
    fill_circle(buffer, win->width, win->height, knob_x, cy, radius, c->fg);
    
    c->render--;

    ui_unlock(&win->lock);

    return 1;
}

void slider_event(component_t* c, event_t ev) {
    slider_data_t* s = (slider_data_t*)c->data;

    int cy = c->y + c->h / 2;
    int radius = (c->h / 2) - 1;
    int knob_x = c->x + ((s->value - s->min) * (c->w - 2 * radius)) / (s->max - s->min) + radius;

    switch (ev.type) {
        case EVENT_MOUSE_LEFT_DOWN: {
            // Clique no knob?
            int dx = ev.mouse.x - knob_x;
            int dy = ev.mouse.y - cy;
            if (dx*dx + dy*dy <= radius*radius) {
                // Clicou dentro do knob → começa arraste
                s->dragging = 1;
            } else if (ev.mouse.x >= c->x && ev.mouse.x <= c->x + c->w &&
                       ev.mouse.y >= c->y && ev.mouse.y <= c->y + c->h) {
                // Clicou na barra → move direto
                int rel = ev.mouse.x - c->x - radius;
                if (rel < 0) rel = 0;
                if (rel > c->w - 2*radius) rel = c->w - 2*radius;
                s->value = s->min + (rel * (s->max - s->min)) / (c->w - 2*radius);
                if (s->on_change) s->on_change(s->value);
                c->render = 1;
                c->ctx->window->invalid = 1;
                s->dragging = 1; // já começa arrastando
            }
        } break;

        case EVENT_MOUSE_MOVE:
            if (s->dragging) {
                int rel = ev.mouse.x - c->x - radius;
                if (rel < 0) rel = 0;
                if (rel > c->w - 2*radius) rel = c->w - 2*radius;
                s->value = s->min + (rel * (s->max - s->min)) / (c->w - 2*radius);
                if (s->on_change) s->on_change(s->value);
                c->render = 1;
                c->ctx->window->invalid = 1;
            }
            break;

        case EVENT_MOUSE_UP:
            s->dragging = 0;
            break;

        default:
            break;
    }
}
component_t* create_slider(ui_context_t* ctx, int x, int y, int w, int h,
                           int min, int max, int value, void (*on_change)(int)) {
    if (ctx->count >= 64) return NULL;

    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = x; comp->y = y; comp->w = w; comp->h = h;
    comp->type = UI_SLIDER;
    comp->visible = 1;
    comp->on_draw = draw_slider;
    comp->on_event = slider_event;
    comp->fg = 0x222222; comp->border_color = 0x666666;
    comp->render = 1;

    slider_data_t* s = malloc(sizeof(slider_data_t));
    s->min = min; s->max = max; s->value = value;
    s->dragging = 0; s->on_change = on_change;
    comp->data = s;

    comp->ctx = ctx;
    ctx->components[ctx->count++] = comp;
    return comp;
}
