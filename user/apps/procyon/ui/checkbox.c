#include "include/components.h"
#include "include/checkbox.h"
#include <stdlib.h>
#include <string.h>


// CHECKBOX
int draw_checkbox(component_t* c, window_t* win) {
    if (!c || !c->visible || !c->render) return 0;

    checkbox_data_t* cb = (checkbox_data_t*)c->data;

    ui_lock(&win->lock);

    // Quadrado da caixa
    draw_rect_component(win->buffer, win->width, win->height,
                        c->x, c->y, c->h, c->h,
                        c->bg, c->border_color, 1, 3);

    // Se marcado, desenha um "X" dentro
    if (cb->checked) {
        draw_line(win->buffer, win->width, win->height, c->x+3, c->y+3, c->x+c->h-3, c->y+c->h-3, c->fg);
        draw_line(win->buffer, win->width, win->height, c->x+c->h-3, c->y+3, c->x+3, c->y+c->h-3, c->fg);
    }

    // Apagar área antiga (background da janela na área da label)
    fill_rect(win->buffer, win->width, win->height, c->x + c->h + 6, c->y, c->w - c->h - 6, c->h, win->bg);
    // Texto ao lado
    int y = c->y + ((c->h - c->font_size)/2);
    draw_text(c->font, cb->label, c->x + c->h + 6, y, c->w - c->h - 6, c->fg, c->font_size, win);

    c->render--;

    ui_unlock(&win->lock);

    return 1;
}

void checkbox_event(component_t* c, event_t ev) {
    checkbox_data_t* cb = (checkbox_data_t*)c->data;

    if (ev.type == EVENT_MOUSE_LEFT_DOWN) {
        int inside = (ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w &&
                      ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h);
        if (inside) {
            cb->checked = !cb->checked;
            c->render = 1;
            c->ctx->window->invalid = 1;
            if (cb->on_change) cb->on_change(cb->checked);
        }
    }
}

component_t* create_checkbox(ui_context_t* ctx, int x, int y, int w, int h, const char* label, int initial, void (*on_change)(int)) {
    if (ctx->count >= 64) return NULL;

    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_CHECKBOX;
    comp->visible = 1;
    comp->on_draw = draw_checkbox;
    comp->on_event = checkbox_event;
    comp->font = *(stbtt_fontinfo *)ctx->window->font;
    comp->font_size = 16;
    comp->fg = 0x000000;
    comp->bg = 0xFFFFFF;
    comp->border_color = 0x444444;
    comp->render = 1;

    checkbox_data_t* cb = malloc(sizeof(checkbox_data_t));
    cb->checked = initial;
    strncpy(cb->label, label, sizeof(cb->label));
    cb->on_change = on_change;
    comp->data = cb;

    comp->ctx = ctx;
    ctx->components[ctx->count++] = comp;
    return comp;
}