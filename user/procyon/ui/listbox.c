#include "include/components.h"
#include "include/listbox.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void listbox_invalidate(component_t *c)
{
    if (!c) return;

    c->render = 1;
    c->dirty = 1;
}

static int listbox_visible_rows(const component_t *c, const listbox_data_t *lb)
{
    if (!c || !lb || lb->item_height <= 0 || c->h <= 0)
        return 1;

    int rows = c->h / lb->item_height;
    return rows > 0 ? rows : 1;
}

static void listbox_clamp_scroll(const component_t *c, listbox_data_t *lb)
{
    if (!c || !lb) return;

    int visible_rows = listbox_visible_rows(c, lb);
    int max_offset = lb->count - visible_rows;

    if (max_offset < 0) max_offset = 0;
    if (lb->scroll_offset < 0) lb->scroll_offset = 0;
    if (lb->scroll_offset > max_offset) lb->scroll_offset = max_offset;
}

static void listbox_free_items(char **items, int count)
{
    if (!items) return;

    for (int i = 0; i < count; i++)
        free(items[i]);

    free(items);
}

static char **listbox_copy_items(const char *items[], int count)
{
    if (count <= 0) return NULL;
    if (!items) return NULL;

    char **copy = calloc((size_t)count, sizeof(char *));
    if (!copy) return NULL;

    for (int i = 0; i < count; i++) {
        const char *value = items[i] ? items[i] : "";
        size_t length = strlen(value);

        copy[i] = malloc(length + 1);
        if (!copy[i]) {
            listbox_free_items(copy, i);
            return NULL;
        }

        memcpy(copy[i], value, length + 1);
    }

    return copy;
}

static void listbox_on_clear(component_t *c)
{
    if (!c || c->type != UI_LISTBOX || !c->data) return;

    listbox_data_t *lb = c->data;

    ui_lock(&lb->spinlock);
    lb->selected = -1;
    lb->focused = 0;
    lb->dragging_scroll = 0;
    lb->drag_offset_y = 0;
    lb->scroll_offset = 0;
    ui_unlock(&lb->spinlock);

    listbox_invalidate(c);
}

static void listbox_on_enable(component_t *c)
{
    if (!c || c->type != UI_LISTBOX || !c->data) return;

    listbox_data_t *lb = c->data;

    ui_lock(&lb->spinlock);
    lb->dragging_scroll = 0;
    lb->focused = 0;
    listbox_clamp_scroll(c, lb);
    ui_unlock(&lb->spinlock);

    listbox_invalidate(c);
}

static void listbox_on_disable(component_t *c)
{
    if (!c || c->type != UI_LISTBOX || !c->data) return;

    listbox_data_t *lb = c->data;

    ui_lock(&lb->spinlock);
    lb->focused = 0;
    lb->dragging_scroll = 0;
    lb->drag_offset_y = 0;
    ui_unlock(&lb->spinlock);

    listbox_invalidate(c);
}

static void listbox_on_destroy(component_t *c)
{
    if (!c) return;

    listbox_data_t *lb = c->data;
    if (!lb) return;

    listbox_free_items(lb->items, lb->count);
    lb->items = NULL;
    lb->count = 0;

    free(lb);
    c->data = NULL;
}

int draw_listbox(component_t *c, window_t *win)
{
    if (!c || !win || !win->buffer || !c->visible ||
        !c->enabled || !c->render || !c->data)
        return 0;

    listbox_data_t *lb = c->data;

    ui_lock(&win->lock);
    ui_lock(&lb->spinlock);

    int visible_rows = listbox_visible_rows(c, lb);
    listbox_clamp_scroll(c, lb);

    fill_rect(
        win->buffer, win->width, win->height,
        c->x, c->y, c->w, c->h, c->bg
    );

    if (lb->count > 0 && lb->items) {
        for (int row = 0; row < visible_rows; row++) {
            int idx = row + lb->scroll_offset;
            if (idx >= lb->count) break;

            int yy = c->y + row * lb->item_height;
            int item_width = c->w - 8;
            if (item_width < 0) item_width = 0;

            unsigned int bg = c->bg;

            if (idx == lb->selected) {
                bg = 0xFFCCE5FF;
                fill_rect(
                    win->buffer, win->width, win->height,
                    c->x, yy, item_width, lb->item_height, bg
                );
                draw_rect_border(
                    win->buffer, win->width, win->height,
                    c->x, yy, item_width, lb->item_height, 0xFF3366FF
                );
            } else {
                fill_rect(
                    win->buffer, win->width, win->height,
                    c->x, yy, item_width, lb->item_height, bg
                );
            }

            if (lb->items[idx]) {
                draw_text_cached(
                    &c->font_cache,
                    win->buffer,
                    win->width,
                    win->height,
                    lb->items[idx],
                    c->x + 4,
                    yy + 2,
                    c->w - 12,
                    c->fg
                );
            }
        }
    }

    draw_rect_border(
        win->buffer, win->width, win->height,
        c->x, c->y, c->w, c->h, 0xFF000000
    );

    if (lb->count > visible_rows && c->w >= 8 && c->h > 0) {
        int track_x = c->x + c->w - 8;
        int bar_h = (c->h * visible_rows) / lb->count;
        if (bar_h < 8) bar_h = 8;
        if (bar_h > c->h) bar_h = c->h;

        int range = lb->count - visible_rows;
        int travel = c->h - bar_h;
        int bar_y = c->y;

        if (range > 0 && travel > 0)
            bar_y += (travel * lb->scroll_offset) / range;

        fill_rect(
            win->buffer, win->width, win->height,
            track_x, c->y, 8, c->h, 0xFFEFEFEF
        );
        draw_rect_border(
            win->buffer, win->width, win->height,
            track_x, c->y, 8, c->h, 0xFF999999
        );

        fill_rect(
            win->buffer, win->width, win->height,
            track_x, bar_y, 8, bar_h, 0xFFBBBBBB
        );
        draw_rect_border(
            win->buffer, win->width, win->height,
            track_x, bar_y, 8, bar_h, 0xFF555555
        );
    }

    c->render = 0;
    c->dirty = 0;
    c->clear = 0;

    ui_unlock(&lb->spinlock);
    ui_unlock(&win->lock);

    return 1;
}

int listbox_event(component_t *c, event_t ev)
{
    if (!c || !c->data || !c->enabled || !c->visible)
        return 0;

    listbox_data_t *lb = c->data;
    void (*on_select)(int, const char *) = NULL;
    char *selected_text = NULL;
    int selected_index = -1;
    int changed = 0;

    ui_lock(&lb->spinlock);

    int visible_rows = listbox_visible_rows(c, lb);
    listbox_clamp_scroll(c, lb);

    int inside = ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w &&
                 ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h;

    if (ev.type == EVENT_MOUSE_LEFT_DOWN) {
        if (inside) {
            lb->focused = 1;
        } else {
            lb->focused = 0;
            lb->dragging_scroll = 0;
            changed = 1;
        }
    }

    if (!lb->focused) {
        if (changed) {
            ui_unlock(&lb->spinlock);
            listbox_invalidate(c);
            return c->render;
        }

        ui_unlock(&lb->spinlock);
        return c->render;
    }

    switch (ev.type) {
        case EVENT_MOUSE_LEFT_DOWN:
            if (!inside) break;

            if (lb->count > visible_rows && c->w >= 8 && c->h > 0) {
                int bar_h = (c->h * visible_rows) / lb->count;
                if (bar_h < 8) bar_h = 8;
                if (bar_h > c->h) bar_h = c->h;

                int range = lb->count - visible_rows;
                int travel = c->h - bar_h;
                int bar_y = c->y;

                if (range > 0 && travel > 0)
                    bar_y += (travel * lb->scroll_offset) / range;

                if (ev.mouse.x >= c->x + c->w - 8 &&
                    ev.mouse.x < c->x + c->w) {
                    if (ev.mouse.y >= bar_y && ev.mouse.y < bar_y + bar_h) {
                        lb->dragging_scroll = 1;
                        lb->drag_offset_y = ev.mouse.y - bar_y;
                        break;
                    }

                    if (travel > 0) {
                        int relative_y = ev.mouse.y - c->y - bar_h / 2;
                        if (relative_y < 0) relative_y = 0;
                        if (relative_y > travel) relative_y = travel;

                        lb->scroll_offset = (relative_y * range) / travel;
                        listbox_clamp_scroll(c, lb);
                        changed = 1;
                    }
                    break;
                }
            }

            if (ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w - 8 &&
                ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h &&
                lb->item_height > 0) {
                int idx = (ev.mouse.y - c->y) / lb->item_height +
                          lb->scroll_offset;

                if (idx >= 0 && idx < lb->count && lb->items) {
                    lb->selected = idx;
                    selected_index = idx;
                    on_select = lb->on_select;

                    if (on_select && lb->items[idx]) {
                        size_t len = strlen(lb->items[idx]);
                        selected_text = malloc(len + 1);
                        if (selected_text)
                            memcpy(selected_text, lb->items[idx], len + 1);
                    }

                    changed = 1;
                }
            }
            break;

        case EVENT_MOUSE_MOVE:
            if (lb->dragging_scroll && lb->count > visible_rows &&
                c->h > 0) {
                int bar_h = (c->h * visible_rows) / lb->count;
                if (bar_h < 8) bar_h = 8;
                if (bar_h > c->h) bar_h = c->h;

                int range = lb->count - visible_rows;
                int travel = c->h - bar_h;

                if (range > 0 && travel > 0) {
                    int new_bar_y = ev.mouse.y - lb->drag_offset_y;
                    int min_y = c->y;
                    int max_y = c->y + travel;

                    if (new_bar_y < min_y) new_bar_y = min_y;
                    if (new_bar_y > max_y) new_bar_y = max_y;

                    lb->scroll_offset =
                        ((new_bar_y - c->y) * range) / travel;
                    listbox_clamp_scroll(c, lb);
                    changed = 1;
                }
            }
            break;

        case EVENT_MOUSE_UP:
            if (lb->dragging_scroll) {
                lb->dragging_scroll = 0;
                changed = 1;
            }
            break;

        case EVENT_KEY_DOWN:
            if (lb->count <= 0 || !lb->items)
                break;

            if (ev.key.keycode == KEY_DOWN) {
                if (lb->selected < 0)
                    lb->selected = 0;
                else if (lb->selected < lb->count - 1)
                    lb->selected++;

                if (lb->selected >= lb->scroll_offset + visible_rows)
                    lb->scroll_offset = lb->selected - visible_rows + 1;

                selected_index = lb->selected;
                on_select = lb->on_select;
                if (on_select && lb->items[selected_index]) {
                    size_t len = strlen(lb->items[selected_index]);
                    selected_text = malloc(len + 1);
                    if (selected_text)
                        memcpy(selected_text, lb->items[selected_index], len + 1);
                }
                changed = 1;
            } else if (ev.key.keycode == KEY_UP) {
                if (lb->selected > 0)
                    lb->selected--;
                else if (lb->selected < 0)
                    lb->selected = 0;

                if (lb->selected < lb->scroll_offset)
                    lb->scroll_offset = lb->selected;

                selected_index = lb->selected;
                on_select = lb->on_select;
                if (on_select && lb->items[selected_index]) {
                    size_t len = strlen(lb->items[selected_index]);
                    selected_text = malloc(len + 1);
                    if (selected_text)
                        memcpy(selected_text, lb->items[selected_index], len + 1);
                }
                changed = 1;
            }
            break;

        default:
            break;
    }

    listbox_clamp_scroll(c, lb);
    ui_unlock(&lb->spinlock);

    if (changed)
        listbox_invalidate(c);

    if (on_select && selected_index >= 0 && selected_text)
        on_select(selected_index, selected_text);

    free(selected_text);
    return c->render;
}

void listbox_set_items(component_t *c, const char *items[], int count)
{
    if (!c || c->type != UI_LISTBOX || !c->data || count < 0)
        return;

    if (count > 0 && !items)
        return;

    char **new_items = listbox_copy_items(items, count);
    if (count > 0 && !new_items)
        return;

    listbox_data_t *lb = c->data;

    ui_lock(&lb->spinlock);

    char **old_items = lb->items;
    int old_count = lb->count;

    lb->items = new_items;
    lb->count = count;
    lb->selected = count > 0 ? 0 : -1;
    lb->scroll_offset = 0;
    lb->dragging_scroll = 0;
    lb->drag_offset_y = 0;

    ui_unlock(&lb->spinlock);

    listbox_free_items(old_items, old_count);
    listbox_invalidate(c);
}

component_t *create_listbox(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    const char *items[],
    int count,
    void (*on_select)(int, const char *))
{
    if (!ctx || !ctx->window || !ctx->window->font ||
        w <= 0 || h <= 0 || count < 0 || (count > 0 && !items))
        return NULL;

    component_t *comp = calloc(1, sizeof(component_t));
    if (!comp) return NULL;

    listbox_data_t *lb = calloc(1, sizeof(listbox_data_t));
    if (!lb) {
        free(comp);
        return NULL;
    }

    if (count > 0) {
        lb->items = listbox_copy_items(items, count);
        if (!lb->items) {
            free(lb);
            free(comp);
            return NULL;
        }
    }

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_LISTBOX;
    comp->visible = 1;
    comp->enabled = 1;
    comp->on_draw = draw_listbox;
    comp->on_event = listbox_event;
    comp->font = *(stbtt_fontinfo *)ctx->window->font;
    comp->font_size = 16;
    comp->fg = 0xFF000000;
    comp->bg = 0xFFFFFFFF;
    comp->border_color = 0xFF333333;
    comp->render = 1;
    comp->dirty = 1;
    comp->data = lb;
    comp->ctx = ctx;

    lb->count = count;
    lb->selected = count > 0 ? 0 : -1;
    lb->on_select = on_select;
    lb->focused = 0;
    lb->item_height = comp->font_size + (comp->font_size / 2);
    lb->scroll_offset = 0;
    lb->dragging_scroll = 0;
    lb->drag_offset_y = 0;

    comp->on_clear = listbox_on_clear;
    comp->on_enable = listbox_on_enable;
    comp->on_disable = listbox_on_disable;
    comp->on_destroy = listbox_on_destroy;

    build_font_cache(&comp->font, comp->font_size, &comp->font_cache);

    if (ui_add_component(ctx, comp)) {
        if(comp->on_destroy) 
            comp->on_destroy(comp);

        free(comp);
        return NULL;
    }
    
    return comp;
}
