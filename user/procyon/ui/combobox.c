
#include "include/components.h"
#include "include/combobox.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>


/* ============================================================
   INVALIDAÇÃO
   ============================================================ */

static void combobox_invalidate(component_t *c)
{
    if (!c || !c->ctx)
        return;

    c->render = 1;
    c->dirty = 1;

}


/* ============================================================
   MODAL
   ============================================================ */

static void combobox_set_modal(component_t *c, int active)
{
    if (!c || !c->ctx)
        return;

    ui_context_t *ctx = c->ctx;

    ui_lock(&ctx->lock);

    if (active) {
        ctx->modal_active = 1;
        ctx->modal_owner = c;
    } else {
        if (ctx->modal_owner == c) {
            ctx->modal_owner = NULL;
            ctx->modal_active = 0;
        }
    }

    ui_unlock(&ctx->lock);
}


/* ============================================================
   FECHAR DROPDOWN
   ============================================================ */

static void combobox_close(component_t *c)
{
    if (!c || !c->data)
        return;

    combobox_data_t *cb = (combobox_data_t*)c->data;

    ui_lock(&cb->lock);

    window_t *dropdown = cb->win;

    cb->win = NULL;
    cb->expanded = 0;
    cb->dragging_scroll = 0;

    ui_unlock(&cb->lock);

    combobox_set_modal(c, 0);

    if (dropdown) {
        if (c->ctx && c->ctx->window) {
            enviar_remocao_janela_filha(
                c->ctx->fd,
                c->ctx->window
            );
        }

        window_destroy(dropdown);
    }

    combobox_invalidate(c);
}


/* ============================================================
   ABRIR DROPDOWN
   ============================================================ */

static int combobox_open(component_t *c)
{
    if (!c || !c->ctx || !c->data)
        return 0;

    combobox_data_t *cb = (combobox_data_t*)c->data;

    if (cb->item_height <= 0)
        return 0;

    ui_lock(&cb->lock);

    if (cb->expanded) {
        ui_unlock(&cb->lock);
        return 1;
    }

    int count = cb->count;
    int item_height = cb->item_height;
    int scroll_offset = cb->scroll_offset;

    ui_unlock(&cb->lock);

    int max_height =
        c->ctx->window->height - c->y - c->h - 8;

    if (max_height < item_height)
        return 0;

    int height;

    if (count > INT_MAX / item_height)
        height = max_height;
    else
        height = count * item_height;

    if (height <= 0)
        height = item_height;

    if (height > max_height)
        height = (max_height / item_height) * item_height;

    if (height <= 0)
        return 0;

    int x = c->ctx->window->x + c->x;
    int y = c->ctx->window->y + c->y + c->h;

    window_t *dropdown = overlay_window_create(
        x,
        y,
        c->w,
        height,
        0xFFC0C0C0,
        0
    );

    if (!dropdown)
        return 0;

    ui_lock(&cb->lock);

    if (cb->expanded || cb->win) {
        ui_unlock(&cb->lock);
        window_destroy(dropdown);
        return 0;
    }

    cb->win = dropdown;
    cb->expanded = 1;
    cb->scroll_offset = scroll_offset;

    ui_unlock(&cb->lock);

    combobox_set_modal(c, 1);

    enviar_janela_filha_ao_servidor(
        c->ctx->fd,
        dropdown,
        c->ctx->window
    );

    combobox_invalidate(c);

    return 1;
}


/* ============================================================
   DESENHO
   ============================================================ */

int draw_combobox(component_t *c, window_t *win)
{
    if (!c || !win || !c->visible || !c->render || !c->data)
        return 0;

    combobox_data_t *cb = (combobox_data_t*)c->data;

    ui_lock(&cb->lock);

    ui_lock(&win->lock);

    /* Caixa principal */
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

    draw_rect_border(
        win->buffer,
        win->width,
        win->height,
        c->x,
        c->y,
        c->w,
        c->h,
        c->border_color
    );

    int yy = c->y + ((c->h - c->font_size) / 2);

    /* Item seleccionado */
    if (cb->count > 0 &&
        cb->selected >= 0 &&
        cb->selected < cb->count &&
        cb->items &&
        cb->items[cb->selected]) {

        draw_text_cached(
            &c->font_cache,
            win->buffer,
            win->width,
            win->height,
            cb->items[cb->selected],
            c->x + 5,
            yy,
            c->w - 25,
            c->fg
        );
    }

    /* Seta */
    draw_text_cached(
        &c->font_cache,
        win->buffer,
        win->width,
        win->height,
        "v",
        c->x + c->w - 15,
        yy,
        15,
        c->fg
    );

    ui_unlock(&win->lock);

    /* Dropdown */
    window_t *dropdown = cb->win;

    if (cb->expanded && dropdown) {

        ui_lock(&dropdown->lock);

        int visible_rows =
            dropdown->height / cb->item_height;

        if (visible_rows < 1)
            visible_rows = 1;

        int max_offset = cb->count > visible_rows
            ? cb->count - visible_rows
            : 0;

        if (cb->scroll_offset < 0)
            cb->scroll_offset = 0;

        if (cb->scroll_offset > max_offset)
            cb->scroll_offset = max_offset;

        fill_rect(
            dropdown->buffer,
            dropdown->width,
            dropdown->height,
            0,
            0,
            dropdown->width,
            dropdown->height,
            c->bg
        );

        for (int i = 0;
             i < visible_rows &&
             i + cb->scroll_offset < cb->count;
             i++) {

            int idx = i + cb->scroll_offset;

            int row_y = i * cb->item_height;

            unsigned int bg = c->bg;

            if (idx == cb->selected) {
                bg = 0xFFCCE5FF;
            }

            fill_rect(
                dropdown->buffer,
                dropdown->width,
                dropdown->height,
                0,
                row_y,
                dropdown->width,
                cb->item_height,
                bg
            );

            if (idx == cb->selected) {
                draw_rect_border(
                    dropdown->buffer,
                    dropdown->width,
                    dropdown->height,
                    0,
                    row_y,
                    dropdown->width,
                    cb->item_height,
                    0xFF3366FF
                );
            }

            int text_y = row_y +
                ((cb->item_height - c->font_size) / 2);

            draw_text_cached(
                &c->font_cache,
                dropdown->buffer,
                dropdown->width,
                dropdown->height,
                cb->items[idx],
                5,
                text_y,
                dropdown->width - 15,
                c->fg
            );
        }

        /* Scrollbar */
        if (cb->count > visible_rows) {

            int bar_h =
                (dropdown->height * visible_rows) / cb->count;

            if (bar_h < 8)
                bar_h = 8;

            if (bar_h > dropdown->height)
                bar_h = dropdown->height;

            int track = dropdown->height - bar_h;

            int bar_y = max_offset > 0
                ? track * cb->scroll_offset / max_offset
                : 0;

            fill_rect(
                dropdown->buffer,
                dropdown->width,
                dropdown->height,
                dropdown->width - 8,
                0,
                8,
                dropdown->height,
                0xFFEFEFEF
            );

            draw_rect_border(
                dropdown->buffer,
                dropdown->width,
                dropdown->height,
                dropdown->width - 8,
                0,
                8,
                dropdown->height,
                0xFF999999
            );

            fill_rect(
                dropdown->buffer,
                dropdown->width,
                dropdown->height,
                dropdown->width - 8,
                bar_y,
                8,
                bar_h,
                0xFFBBBBBB
            );

            draw_rect_border(
                dropdown->buffer,
                dropdown->width,
                dropdown->height,
                dropdown->width - 8,
                bar_y,
                8,
                bar_h,
                0xFF555555
            );
        }

        draw_rect_border(
            dropdown->buffer,
            dropdown->width,
            dropdown->height,
            0,
            0,
            dropdown->width,
            dropdown->height,
            0xFF000000
        );

        ui_unlock(&dropdown->lock);
    }

    c->render = 0;
    c->dirty = 0;
    c->clear = 0;

    ui_unlock(&cb->lock);

    return 1;
}


/* ============================================================
   EVENTOS
   ============================================================ */

int combobox_event(component_t *c, event_t ev)
{
    if (!c || !c->visible || !c->enabled || !c->data)
        return 0;

    combobox_data_t *cb = (combobox_data_t*)c->data;

    int inside_main = (
        ev.mouse.x >= c->x &&
        ev.mouse.x < c->x + c->w &&
        ev.mouse.y >= c->y &&
        ev.mouse.y < c->y + c->h
    );

    /* Abrir ou fechar */
    if (ev.type == EVENT_MOUSE_LEFT_DOWN && inside_main) {

        ui_lock(&cb->lock);
        int expanded = cb->expanded;
        ui_unlock(&cb->lock);

        if (expanded)
            combobox_close(c);
        else
            combobox_open(c);

        return 1;
    }

    ui_lock(&cb->lock);

    if (!cb->expanded || !cb->win) {
        ui_unlock(&cb->lock);
        return 0;
    }

    int visible_rows = cb->win->height / cb->item_height;

    if (visible_rows < 1)
        visible_rows = 1;

    int rel_x = ev.mouse.x - c->x;
    int rel_y = ev.mouse.y - (c->y + c->h);

    int max_offset = cb->count > visible_rows
        ? cb->count - visible_rows
        : 0;

    int selected = -1;
    const char *selected_text = NULL;

    int changed = 0;
    int close_dropdown = 0;

    switch (ev.type) {

        case EVENT_MOUSE_LEFT_DOWN:

            /* Scrollbar */
            if (cb->count > visible_rows &&
                rel_x >= c->w - 8 &&
                rel_x < c->w) {

                int bar_h =
                    (cb->win->height * visible_rows) / cb->count;

                if (bar_h < 8)
                    bar_h = 8;

                if (bar_h > cb->win->height)
                    bar_h = cb->win->height;

                int track = cb->win->height - bar_h;

                int bar_y = max_offset > 0
                    ? track * cb->scroll_offset / max_offset
                    : 0;

                if (rel_y >= bar_y && rel_y < bar_y + bar_h) {

                    cb->dragging_scroll = 1;
                    cb->drag_offset_y = rel_y - bar_y;

                } else if (track > 0) {

                    int pos = rel_y * max_offset / track;

                    if (pos < 0)
                        pos = 0;

                    if (pos > max_offset)
                        pos = max_offset;

                    cb->scroll_offset = pos;
                    changed = 1;
                }

                break;
            }

            /* Seleccionar item */
            if (rel_x >= 0 &&
                rel_x < c->w - 8 &&
                rel_y >= 0 &&
                rel_y < cb->win->height) {

                int idx =
                    (rel_y / cb->item_height) + cb->scroll_offset;

                if (idx >= 0 && idx < cb->count) {

                    cb->selected = idx;

                    selected = idx;
                    selected_text = cb->items[idx];

                    changed = 1;
                    close_dropdown = 1;
                }

            } else {
                /* Clique fora */
                close_dropdown = 1;
            }

            break;

        case EVENT_MOUSE_MOVE:

            if (cb->dragging_scroll && max_offset > 0) {

                int bar_h =
                    (cb->win->height * visible_rows) / cb->count;

                if (bar_h < 8)
                    bar_h = 8;

                if (bar_h > cb->win->height)
                    bar_h = cb->win->height;

                int track = cb->win->height - bar_h;

                if (track > 0) {

                    int new_bar_y = rel_y - cb->drag_offset_y;

                    if (new_bar_y < 0)
                        new_bar_y = 0;

                    if (new_bar_y > track)
                        new_bar_y = track;

                    int pos = new_bar_y * max_offset / track;

                    if (pos != cb->scroll_offset) {
                        cb->scroll_offset = pos;
                        changed = 1;
                    }
                }
            }

            break;

        case EVENT_MOUSE_UP:

            cb->dragging_scroll = 0;
            break;

        case EVENT_KEY_DOWN:

            if (ev.key.keycode == KEY_DOWN &&
                cb->selected < cb->count - 1) {

                cb->selected++;

                if (cb->selected >= cb->scroll_offset + visible_rows)
                    cb->scroll_offset++;

                selected = cb->selected;
                selected_text = cb->items[selected];

                changed = 1;

            } else if (ev.key.keycode == KEY_UP &&
                       cb->selected > 0) {

                cb->selected--;

                if (cb->selected < cb->scroll_offset)
                    cb->scroll_offset--;

                selected = cb->selected;
                selected_text = cb->items[selected];

                changed = 1;
            }

            break;

        default:
            break;
    }

    void (*on_select)(int, const char*) = cb->on_select;

    ui_unlock(&cb->lock);

    if (changed)
        combobox_invalidate(c);

    if (close_dropdown)
        combobox_close(c);

    /* Callback sempre fora do spinlock */
    if (selected >= 0 && on_select && selected_text)
        on_select(selected, selected_text);

    return changed || close_dropdown;
}


/* ============================================================
   LIMPAR
   ============================================================ */

static void combobox_clear(component_t *c)
{
    if (!c || !c->data)
        return;

    combobox_data_t *cb = (combobox_data_t*)c->data;

    ui_lock(&cb->lock);

    cb->selected = -1;
    cb->scroll_offset = 0;
    cb->dragging_scroll = 0;

    ui_unlock(&cb->lock);

    combobox_close(c);
}


/* ============================================================
   DESACTIVAR
   ============================================================ */

static void combobox_disable(component_t *c)
{
    if (!c || !c->data)
        return;

    combobox_close(c);
}


/* ============================================================
   DESTRUIR
   ============================================================ */

static void combobox_destroy(component_t *c)
{
    if (!c || !c->data)
        return;

    combobox_data_t *cb = (combobox_data_t*)c->data;

    combobox_close(c);

    if (cb->items) {
        for (int i = 0; i < cb->count; i++)
            free(cb->items[i]);

        free(cb->items);
    }

    free(cb);

    c->data = NULL;
}


/* ============================================================
   CRIAR COMBOBOX
   ============================================================ */

component_t* create_combobox(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    const char *items[],
    int count,
    void (*on_select)(int, const char*)
)
{
    if (!ctx || !ctx->window || count < 0 || (count > 0 && !items))
        return NULL;

    component_t *comp = calloc(1, sizeof(component_t));

    if (!comp)
        return NULL;

    combobox_data_t *cb = calloc(1, sizeof(combobox_data_t));

    if (!cb) {
        free(comp);
        return NULL;
    }

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;

    comp->type = UI_COMBOBOX;

    comp->visible = 1;
    comp->enabled = 1;

    comp->on_draw = draw_combobox;
    comp->on_event = combobox_event;

    comp->on_clear = combobox_clear;
    comp->on_disable = combobox_disable;
    comp->on_destroy = combobox_destroy;

    if (ctx->window->font)
        comp->font = *(stbtt_fontinfo*)ctx->window->font;

    comp->font_size = (int)(h * 0.8f);

    comp->fg = 0xFF000000;
    comp->bg = 0xFFFFFFFF;
    comp->border_color = 0xFF333333;

    comp->render = 1;
    comp->dirty = 1;

    cb->lock = 0;
    cb->count = count;
    cb->selected = count > 0 ? 0 : -1;
    cb->expanded = 0;
    cb->on_select = on_select;
    cb->item_height = h;
    cb->scroll_offset = 0;
    cb->dragging_scroll = 0;
    cb->win = NULL;

    if (count > 0) {

        cb->items = calloc(count, sizeof(char*));

        if (!cb->items)
            goto fail;

        for (int i = 0; i < count; i++) {

            if (!items[i])
                continue;

            cb->items[i] = calloc(256, 1);

            if (!cb->items[i])
                goto fail;

            strncpy(cb->items[i], items[i], 255);
        }
    }

    comp->data = cb;
    comp->ctx = ctx;

    build_font_cache(
        &comp->font,
        comp->font_size,
        &comp->font_cache
    );

    if (ui_add_component(ctx, comp) != 0)
        goto fail;

    return comp;

fail:

    if (cb->items) {
        for (int i = 0; i < count; i++)
            free(cb->items[i]);

        free(cb->items);
    }

    free(cb);
    free(comp);

    return NULL;
}


/* ============================================================
   ALTERAR SELECÇÃO
   ============================================================ */

void combobox_set_selected(component_t *c, int selected)
{
    if (!c || c->type != UI_COMBOBOX || !c->data)
        return;

    combobox_data_t *cb = (combobox_data_t*)c->data;

    ui_lock(&cb->lock);

    if (selected < -1 || selected >= cb->count) {
        ui_unlock(&cb->lock);
        return;
    }

    if (cb->selected == selected) {
        ui_unlock(&cb->lock);
        return;
    }

    cb->selected = selected;

    ui_unlock(&cb->lock);

    combobox_invalidate(c);
}


/* ============================================================
   OBTER SELECÇÃO
   ============================================================ */

int combobox_get_selected(component_t *c)
{
    if (!c || c->type != UI_COMBOBOX || !c->data)
        return -1;

    combobox_data_t *cb = (combobox_data_t*)c->data;

    ui_lock(&cb->lock);

    int selected = cb->selected;

    ui_unlock(&cb->lock);

    return selected;
}
