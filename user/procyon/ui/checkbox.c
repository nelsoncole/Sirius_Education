
#include "include/components.h"
#include "include/checkbox.h"
#include <stdlib.h>
#include <string.h>


/* ============================================================
   INVALIDAÇÃO
   ============================================================ */

static void checkbox_invalidate(component_t *c)
{
    if (!c) return;

    c->render = 1;
    c->dirty = 1;
}


/* ============================================================
   DESENHO
   ============================================================ */

int draw_checkbox(component_t* c, window_t* win)
{
    if (!c || !win || !c->visible || !c->render || !c->data)
        return 0;

    checkbox_data_t* cb = (checkbox_data_t*)c->data;

    ui_lock(&win->lock);

    /* Caixa */
    draw_rect_component(
        win->buffer,
        win->width,
        win->height,
        c->x, c->y,
        c->h, c->h,
        c->bg,
        c->border_color,
        1,
        3
    );

    /* Marca */
    if (cb->checked) {
        draw_line(
            win->buffer,
            win->width,
            win->height,
            c->x + 3,
            c->y + 3,
            c->x + c->h - 3,
            c->y + c->h - 3,
            c->fg
        );

        draw_line(
            win->buffer,
            win->width,
            win->height,
            c->x + c->h - 3,
            c->y + 3,
            c->x + 3,
            c->y + c->h - 3,
            c->fg
        );
    }

    /* Área da label */
    int label_x = c->x + c->h + 6;
    int label_w = c->w - c->h - 6;

    if (label_w > 0) {
        fill_rect(
            win->buffer,
            win->width,
            win->height,
            label_x,
            c->y,
            label_w,
            c->h,
            win->bg
        );

        int text_y = c->y + ((c->h - c->font_size) / 2);

        draw_text(
            c->font,
            cb->label,
            label_x,
            text_y,
            label_w,
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


/* ============================================================
   EVENTOS
   ============================================================ */

int checkbox_event(component_t* c, event_t ev)
{
    if (!c || !c->enabled || !c->visible || !c->data)
        return 0;

    checkbox_data_t* cb = (checkbox_data_t*)c->data;

    if (ev.type != EVENT_MOUSE_LEFT_DOWN)
        return 0;

    int inside = (
        ev.mouse.x >= c->x &&
        ev.mouse.x < c->x + c->w &&
        ev.mouse.y >= c->y &&
        ev.mouse.y < c->y + c->h
    );

    if (!inside)
        return 0;

    cb->checked = !cb->checked;

    int checked = cb->checked;

    checkbox_invalidate(c);

    /* Callback fora do lock do gestor */
    if (cb->on_change)
        cb->on_change(checked);

    return 1;
}


/* ============================================================
   LIMPAR
   ============================================================ */

static void checkbox_clear(component_t *c)
{
    if (!c || !c->data)
        return;

    checkbox_data_t* cb = (checkbox_data_t*)c->data;

    cb->checked = 0;
}


/* ============================================================
   DESACTIVAR
   ============================================================ */

static void checkbox_disable(component_t *c)
{
    if (!c)
        return;

    /* Não altera o estado checked.
       Apenas impede interacções. */
}


/* ============================================================
   DESTRUIR
   ============================================================ */

static void checkbox_destroy(component_t *c)
{
    if (!c)
        return;

    free(c->data);
    c->data = NULL;
}


/* ============================================================
   CRIAR CHECKBOX
   ============================================================ */

component_t* create_checkbox(
    ui_context_t* ctx,
    int x,
    int y,
    int w,
    int h,
    const char* label,
    int initial,
    void (*on_change)(int)
)
{
    if (!ctx || !ctx->window)
        return NULL;

    component_t* comp = calloc(1, sizeof(component_t));

    if (!comp)
        return NULL;

    checkbox_data_t* cb = calloc(1, sizeof(checkbox_data_t));

    if (!cb) {
        free(comp);
        return NULL;
    }

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;

    comp->type = UI_CHECKBOX;

    comp->visible = 1;
    comp->enabled = 1;

    comp->on_draw = draw_checkbox;
    comp->on_event = checkbox_event;

    comp->on_clear = checkbox_clear;
    comp->on_disable = checkbox_disable;
    comp->on_destroy = checkbox_destroy;

    if (ctx->window->font)
        comp->font = *(stbtt_fontinfo*)ctx->window->font;

    comp->font_size = 16;

    comp->fg = 0xFF000000;
    comp->bg = 0xFFFFFFFF;
    comp->border_color = 0xFF444444;

    comp->render = 1;
    comp->dirty = 1;

    cb->checked = initial ? 1 : 0;

    if (label) {
        strncpy(cb->label, label, sizeof(cb->label) - 1);
        cb->label[sizeof(cb->label) - 1] = '\0';
    }

    cb->on_change = on_change;

    comp->data = cb;
    comp->ctx = ctx;

    if (ui_add_component(ctx, comp)) {
        if(comp->on_destroy) 
            comp->on_destroy(comp);

        free(comp);
        return NULL;
    }

    return comp;
}


/* ============================================================
   ALTERAR ESTADO
   ============================================================ */

void checkbox_set_checked(component_t *c, int checked)
{
    if (!c || c->type != UI_CHECKBOX || !c->data)
        return;

    checkbox_data_t* cb = (checkbox_data_t*)c->data;

    checked = checked ? 1 : 0;

    if (cb->checked == checked)
        return;

    cb->checked = checked;

    checkbox_invalidate(c);
}


/* ============================================================
   OBTER ESTADO
   ============================================================ */

int checkbox_is_checked(component_t *c)
{
    if (!c || c->type != UI_CHECKBOX || !c->data)
        return 0;

    checkbox_data_t* cb = (checkbox_data_t*)c->data;

    return cb->checked;
}