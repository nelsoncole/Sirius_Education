#include "include/label.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


// LABEL
int draw_label(component_t* c, window_t* win) {
    if (!c || !c->visible || !c->render) return 0;

    label_data_t* l = (label_data_t*)c->data;

    ui_lock(&win->lock);

    // Apagar área antiga (background da janela na área da label)
    fill_rect(win->buffer, win->width, win->height, c->x, c->y, c->w, c->h, c->bg);

    // Texto alinhado à esquerda
    draw_text(c->font, l->text, c->x, c->y, c->w, c->fg, c->font_size, win);

	c->render--;

    ui_unlock(&win->lock);

    return 1;
}

void label_set_text(component_t* c, const char* new_text) {
    if (!c || !new_text || !c->visible) return;

    label_data_t* l = (label_data_t*)c->data;
    if (!l) return;


    // Atualizar o texto interno
    strncpy(l->text, new_text, sizeof(l->text) - 1);
    l->text[sizeof(l->text) - 1] = '\0';

    c->render = 1;
    c->ctx->window->invalid = 1;
}

component_t* create_label(ui_context_t* ctx, int x, int y, int w, const char* text, unsigned int color) {
    if (ctx->count >= 64) return NULL;

    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = ctx->window->client.x + x;
    comp->y = ctx->window->client.y + y;
    comp->w = w; // largura automática (opcional)
    comp->h = 16;
    comp->type = UI_LABEL;
    comp->visible = 1;
    comp->on_draw = draw_label;
	comp->on_event = 0;
    comp->font = *(stbtt_fontinfo *)ctx->window->font;
    comp->font_size = 16;
    comp->fg = color;
    comp->bg = ctx->window->bg;
	comp->render = 1;

    label_data_t* lbl = malloc(sizeof(label_data_t));
    strncpy(lbl->text, text, sizeof(lbl->text));
    comp->data = lbl;

    comp->ctx = ctx;
    ctx->components[ctx->count++] = comp;
    return comp;
}