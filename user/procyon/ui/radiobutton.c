#include "include/components.h"
#include "include/radiobutton.h"
#include <stdlib.h>
#include <string.h>

// RADIOBUTTON
int draw_radiobutton(component_t* c, window_t* win) {
    if (!c || !c->visible || !c->render) return 0;

    radiobutton_data_t* rb = (radiobutton_data_t*)c->data;

    ui_lock(&win->lock);

    // Círculo externo (caixa redonda)
    draw_rect_component(win->buffer, win->width, win->height,
                        c->x, c->y, c->h, c->h,
                        c->bg, c->border_color, 1, c->h/2);

    // Se marcado → desenhar círculo interno
    if (rb->selected) {
        int r = c->h/2 - 4;
        fill_circle(win->buffer, win->width, win->height, c->x + c->h/2, c->y + c->h/2, r, c->fg);
    }

    // Limpar fundo do texto para evitar sujeira
    fill_rect(win->buffer, win->width, win->height, c->x + c->h + 6, c->y, c->w - c->h - 6, c->h, win->bg);

    // Texto
    int ty = c->y + ((c->h - c->font_size) / 2);
    draw_text(c->font, rb->label, c->x + c->h + 6, ty,
              c->w - c->h - 6, c->fg, c->font_size, win);

    c->render--;

    ui_unlock(&win->lock);

    return 1;
}

void radiobutton_event(component_t* c, event_t ev) {
    if (ev.type == EVENT_MOUSE_LEFT_DOWN) {
        radiobutton_data_t* rb = (radiobutton_data_t*)c->data;
        int inside = (ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w &&
                      ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h);
        if (inside && !rb->selected) {
            // Desmarcar todos do mesmo grupo
            ui_context_t* ctx = c->ctx;
            for (int i = 0; i < ctx->count; i++) {
                component_t* other = ctx->components[i];
                if (other->type == UI_RADIOBUTTON) {
                    radiobutton_data_t* or = (radiobutton_data_t*)other->data;
                    if (or->group_id == rb->group_id) {
                        or->selected = 0;
                        other->render = 1;
                        c->ctx->window->invalid = 1;
                    }
                }
            }
            rb->selected = 1;
            c->render = 1;
            c->ctx->window->invalid = 1;
            if (rb->on_select) rb->on_select(rb->group_id);
        }
    }
}

component_t* create_radiobutton(ui_context_t* ctx, int x, int y, int w, int h,
                                const char* label, int group, int selected,
                                void (*on_select)(int)) {
    if (ctx->count >= 64) return NULL;

    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = x; comp->y = y; comp->w = w; comp->h = h;
    comp->type = UI_RADIOBUTTON;
    comp->visible = 1;
    comp->on_draw = draw_radiobutton;
    comp->on_event = radiobutton_event;
    comp->font = *(stbtt_fontinfo *)ctx->window->font;
    comp->font_size = 16;
    comp->fg = 0x000000; 
    comp->bg = 0xFFFFFFFF;
    comp->border_color = 0x444444; 
    comp->render = 1;

    radiobutton_data_t* rb = malloc(sizeof(radiobutton_data_t));
    strncpy(rb->label, label, sizeof(rb->label) - 1);
    rb->label[sizeof(rb->label) - 1] = '\0';
    rb->selected = selected;
    rb->group_id = group;
    rb->on_select = on_select;
    comp->data = rb;

    comp->ctx = ctx;
    ctx->components[ctx->count++] = comp;
    return comp;
}
