#include "include/components.h"
#include "include/gridview.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define GRIDVIEW_SCROLLBAR_WIDTH 8
#define GRIDVIEW_MIN_THUMB_HEIGHT 8

static void gridview_invalidate(component_t *c)
{
    if (!c) return;
    c->render = 1;
    c->dirty = 1;
}

static int gridview_visible_rows(const component_t *c, const gridview_data_t *gv)
{
    if (!c || !gv || gv->row_height <= 0 || c->h <= gv->row_height)
        return 1;

    int rows = (c->h - gv->row_height) / gv->row_height;
    return rows > 0 ? rows : 1;
}

static int gridview_max_scroll(const gridview_data_t *gv, int visible_rows)
{
    if (!gv || visible_rows < 1 || gv->row_count <= 1) return 0;
    int data_rows = gv->row_count - 1;
    return data_rows > visible_rows ? data_rows - visible_rows : 0;
}

static void gridview_clamp_scroll(gridview_data_t *gv, int visible_rows)
{
    if (!gv) return;
    int max_scroll = gridview_max_scroll(gv, visible_rows);
    if (gv->scroll_offset < 0) gv->scroll_offset = 0;
    if (gv->scroll_offset > max_scroll) gv->scroll_offset = max_scroll;
}

int draw_gridview(component_t *c, window_t *win)
{
    if (!c || !win || !win->buffer || !c->visible || !c->enabled || !c->render || !c->data)
        return 0;

    gridview_data_t *gv = c->data;
    ui_lock(&win->lock);
    ui_lock(&gv->spinlock);

    if (c->w <= 0 || c->h <= 0 || gv->row_height <= 0 || gv->col_count <= 0 ||
        !gv->columns || gv->row_count < 0) {
        c->render = 0;
        c->dirty = 0;
        ui_unlock(&gv->spinlock);
        ui_unlock(&win->lock);
        return 0;
    }

    int header_h = gv->row_height < c->h ? gv->row_height : c->h;
    int data_area_h = c->h - header_h;
    int visible_rows = data_area_h / gv->row_height;
    if (visible_rows < 1) visible_rows = 1;

    int data_rows = gv->row_count > 1 ? gv->row_count - 1 : 0;
    int has_scroll = data_rows > visible_rows;
    int content_w = c->w - (has_scroll ? GRIDVIEW_SCROLLBAR_WIDTH : 0);
    if (content_w < 1) content_w = 1;
    int col_width = content_w / gv->col_count;
    if (col_width < 1) col_width = 1;

    gridview_clamp_scroll(gv, visible_rows);

    fill_rect(win->buffer, win->width, win->height, c->x, c->y, c->w, c->h, c->bg);

    fill_rect(win->buffer, win->width, win->height, c->x, c->y,
              content_w, header_h, 0xFFD0D0D0);

    for (int col = 0; col < gv->col_count; col++) {
        if (gv->columns[0] && gv->columns[0][col]) {
            int cell_x = c->x + col * col_width;
            int cell_w = (col == gv->col_count - 1) ? content_w - col * col_width : col_width;
            if (cell_w > 4)
                draw_text_cached(&c->font_cache, win->buffer, win->width, win->height,
                                 gv->columns[0][col], cell_x + 4, c->y + 2,
                                 cell_w - 4, c->fg);
        }
    }
    draw_rect_border(win->buffer, win->width, win->height,
                     c->x, c->y, content_w, header_h, c->border_color);

    for (int visible_idx = 0; visible_idx < visible_rows; visible_idx++) {
        int row_idx = visible_idx + gv->scroll_offset + 1;
        if (row_idx >= gv->row_count) break;

        int yy = c->y + header_h + visible_idx * gv->row_height;
        if (yy >= c->y + c->h) break;
        int row_h = gv->row_height;
        if (yy + row_h > c->y + c->h) row_h = c->y + c->h - yy;

        unsigned int bg = ((row_idx & 1) == 0) ? 0xFFFFFFFF : 0xFFF8F8F8;
        if (row_idx == gv->selected) bg = 0xFFCCE5FF;

        fill_rect(win->buffer, win->width, win->height,
                  c->x, yy, content_w, row_h, bg);

        for (int col = 0; col < gv->col_count; col++) {
            if (!gv->columns[row_idx] || !gv->columns[row_idx][col]) continue;
            int cell_x = c->x + col * col_width;
            int cell_w = (col == gv->col_count - 1) ? content_w - col * col_width : col_width;
            if (cell_w > 4)
                draw_text_cached(&c->font_cache, win->buffer, win->width, win->height,
                                 gv->columns[row_idx][col], cell_x + 4, yy + 2,
                                 cell_w - 4, c->fg);
        }

        if (row_idx == gv->selected)
            draw_rect_border(win->buffer, win->width, win->height,
                             c->x, yy, content_w, row_h, 0xFF3366FF);
    }

    if (has_scroll && data_area_h > 0) {
        int track_y = c->y + header_h;
        int track_h = data_area_h;
        int thumb_h = (int)(((long long)track_h * visible_rows) / data_rows);
        if (thumb_h < GRIDVIEW_MIN_THUMB_HEIGHT) thumb_h = GRIDVIEW_MIN_THUMB_HEIGHT;
        if (thumb_h > track_h) thumb_h = track_h;
        int range = data_rows - visible_rows;
        int thumb_range = track_h - thumb_h;
        int thumb_y = track_y;
        if (range > 0 && thumb_range > 0)
            thumb_y += (int)(((long long)thumb_range * gv->scroll_offset) / range);

        fill_rect(win->buffer, win->width, win->height,
                  c->x + content_w, track_y, c->w - content_w, track_h, 0xFFEFEFEF);
        draw_rect_border(win->buffer, win->width, win->height,
                         c->x + content_w, track_y, c->w - content_w, track_h, 0xFF999999);
        fill_rect(win->buffer, win->width, win->height,
                  c->x + content_w, thumb_y, c->w - content_w, thumb_h, 0xFFBBBBBB);
        draw_rect_border(win->buffer, win->width, win->height,
                         c->x + content_w, thumb_y, c->w - content_w, thumb_h, 0xFF555555);
    }

    draw_rect_border(win->buffer, win->width, win->height,
                     c->x, c->y, c->w, c->h, c->border_color);

    c->render = 0;
    c->dirty = 0;
    c->clear = 0;
    ui_unlock(&gv->spinlock);
    ui_unlock(&win->lock);
    return 1;
}

int gridview_event(component_t *c, event_t ev)
{
    if (!c || !c->data || !c->enabled || !c->visible) return 0;

    gridview_data_t *gv = c->data;
    int notify = 0;
    int selected = -1;
    char **selected_row = NULL;
    void (*on_select)(int, char **) = NULL;

    ui_lock(&gv->spinlock);

    if (gv->row_height <= 0 || gv->col_count <= 0) {
        ui_unlock(&gv->spinlock);
        return 0;
    }

    int visible_rows = gridview_visible_rows(c, gv);
    int inside = ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w &&
                 ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h;

    if (ev.type == EVENT_MOUSE_LEFT_DOWN) {
        gv->focused = inside ? 1 : 0;
        gv->dragging_scroll = 0;

        if (inside && gv->row_count > 1) {
            int header_h = gv->row_height < c->h ? gv->row_height : c->h;
            int data_rows = gv->row_count - 1;
            int has_scroll = data_rows > visible_rows;
            int content_w = c->w - (has_scroll ? GRIDVIEW_SCROLLBAR_WIDTH : 0);
            if (content_w < 1) content_w = 1;

            if (has_scroll && ev.mouse.x >= c->x + content_w && ev.mouse.y >= c->y + header_h) {
                int track_h = c->h - header_h;
                int thumb_h = track_h > 0 ? (int)(((long long)track_h * visible_rows) / data_rows) : 0;
                if (thumb_h < GRIDVIEW_MIN_THUMB_HEIGHT) thumb_h = GRIDVIEW_MIN_THUMB_HEIGHT;
                if (thumb_h > track_h) thumb_h = track_h;
                int max_scroll = gridview_max_scroll(gv, visible_rows);
                int thumb_range = track_h - thumb_h;
                int thumb_y = c->y + header_h;
                if (max_scroll > 0 && thumb_range > 0)
                    thumb_y += (int)(((long long)thumb_range * gv->scroll_offset) / max_scroll);

                if (ev.mouse.y >= thumb_y && ev.mouse.y < thumb_y + thumb_h) {
                    gv->dragging_scroll = 1;
                    gv->drag_offset_y = ev.mouse.y - thumb_y;
                } else if (thumb_range > 0) {
                    int target_y = ev.mouse.y - thumb_h / 2;
                    if (target_y < c->y + header_h) target_y = c->y + header_h;
                    if (target_y > c->y + c->h - thumb_h) target_y = c->y + c->h - thumb_h;
                    gv->scroll_offset = (int)(((long long)(target_y - c->y - header_h) * max_scroll) / thumb_range);
                    gridview_clamp_scroll(gv, visible_rows);
                    gridview_invalidate(c);
                }
            } else if (ev.mouse.x < c->x + content_w && ev.mouse.y >= c->y + header_h) {
                int row_idx = (ev.mouse.y - c->y - header_h) / gv->row_height + gv->scroll_offset + 1;
                if (row_idx >= 1 && row_idx < gv->row_count) {
                    gv->selected = row_idx;
                    selected = row_idx;
                    selected_row = gv->columns ? gv->columns[row_idx] : NULL;
                    notify = selected_row != NULL;
                    on_select = gv->on_select;
                    gridview_invalidate(c);
                }
            }
        }
    } else if (!gv->focused) {
        ui_unlock(&gv->spinlock);
        return 0;
    } else {
        switch (ev.type) {
            case EVENT_MOUSE_MOVE:
                if (gv->dragging_scroll) {
                    int header_h = gv->row_height < c->h ? gv->row_height : c->h;
                    int data_rows = gv->row_count - 1;
                    int track_h = c->h - header_h;
                    if (data_rows > visible_rows && track_h > 0) {
                        int thumb_h = (int)(((long long)track_h * visible_rows) / data_rows);
                        if (thumb_h < GRIDVIEW_MIN_THUMB_HEIGHT) thumb_h = GRIDVIEW_MIN_THUMB_HEIGHT;
                        if (thumb_h > track_h) thumb_h = track_h;
                        int thumb_range = track_h - thumb_h;
                        int max_scroll = gridview_max_scroll(gv, visible_rows);
                        if (thumb_range > 0 && max_scroll > 0) {
                            int target_y = ev.mouse.y - gv->drag_offset_y;
                            int min_y = c->y + header_h;
                            int max_y = min_y + thumb_range;
                            if (target_y < min_y) target_y = min_y;
                            if (target_y > max_y) target_y = max_y;
                            gv->scroll_offset = (int)(((long long)(target_y - min_y) * max_scroll) / thumb_range);
                            gridview_invalidate(c);
                        }
                    }
                }
                break;

            case EVENT_MOUSE_UP:
                gv->dragging_scroll = 0;
                break;

            case EVENT_KEY_DOWN:
                if (gv->row_count > 1 && (ev.key.keycode == KEY_DOWN || ev.key.keycode == KEY_UP)) {
                    if (ev.key.keycode == KEY_DOWN && gv->selected < gv->row_count - 1) {
                        if (gv->selected < 1) gv->selected = 1;
                        else gv->selected++;
                    } else if (ev.key.keycode == KEY_UP && gv->selected > 1) {
                        gv->selected--;
                    }

                    if (gv->selected >= 1 && gv->selected < gv->row_count) {
                        if (gv->selected - 1 < gv->scroll_offset)
                            gv->scroll_offset = gv->selected - 1;
                        else if (gv->selected - 1 >= gv->scroll_offset + visible_rows)
                            gv->scroll_offset = gv->selected - visible_rows;
                        gridview_clamp_scroll(gv, visible_rows);
                        selected = gv->selected;
                        selected_row = gv->columns ? gv->columns[selected] : NULL;
                        notify = selected_row != NULL;
                        on_select = gv->on_select;
                        gridview_invalidate(c);
                    }
                }
                break;

            default:
                break;
        }
    }

    int result = c->render;
    ui_unlock(&gv->spinlock);

    if (notify && on_select)
        on_select(selected, selected_row);

    return result;
}

void gridview_set_items(component_t *c, char ***new_data, int rows, int cols)
{
    if (!c || !c->data || rows < 0 || cols < 0 || (rows > 0 && cols > 0 && !new_data))
        return;

    gridview_data_t *gv = c->data;
    ui_lock(&gv->spinlock);

    gv->columns = new_data;
    gv->row_count = rows;
    gv->col_count = cols;
    gv->selected = rows > 1 ? 1 : -1;
    gv->scroll_offset = 0;
    gv->dragging_scroll = 0;

    gridview_invalidate(c);
    ui_unlock(&gv->spinlock);
}

static void gridview_on_clear(component_t *c)
{
    if (!c || !c->data) return;
    gridview_data_t *gv = c->data;
    ui_lock(&gv->spinlock);
    gv->selected = -1;
    gv->scroll_offset = 0;
    gv->focused = 0;
    gv->dragging_scroll = 0;
    gridview_invalidate(c);
    ui_unlock(&gv->spinlock);
}

static void gridview_on_enable(component_t *c)
{
    if (!c || !c->data) return;
    gridview_data_t *gv = c->data;
    ui_lock(&gv->spinlock);
    gv->focused = 0;
    gv->dragging_scroll = 0;
    gridview_invalidate(c);
    ui_unlock(&gv->spinlock);
}

static void gridview_on_disable(component_t *c)
{
    if (!c || !c->data) return;
    gridview_data_t *gv = c->data;
    ui_lock(&gv->spinlock);
    gv->focused = 0;
    gv->dragging_scroll = 0;
    gridview_invalidate(c);
    ui_unlock(&gv->spinlock);
}

static void gridview_on_destroy(component_t *c)
{
    if (!c || !c->data) return;
    gridview_data_t *gv = c->data;
    c->data = NULL;
    free(gv);
}

component_t *create_gridview(ui_context_t *ctx, int x, int y, int w, int h,
                             char ***data, int rows, int cols,
                             void (*on_select)(int, char **))
{
    if (!ctx || !ctx->window || !ctx->window->font || w <= 0 || h <= 0 ||
        rows < 0 || cols < 0 || (rows > 0 && cols > 0 && !data))
        return NULL;

    component_t *c = calloc(1, sizeof(component_t));
    if (!c) return NULL;

    gridview_data_t *gv = calloc(1, sizeof(gridview_data_t));
    if (!gv) {
        free(c);
        return NULL;
    }

    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;
    c->type = UI_LISTBOX;
    c->visible = 1;
    c->enabled = 1;
    c->on_draw = draw_gridview;
    c->on_event = gridview_event;
    c->font = *(stbtt_fontinfo *)ctx->window->font;
    c->font_size = 16;
    c->fg = 0xFF000000;
    c->bg = 0xFFFFFFFF;
    c->border_color = 0xFF333333;
    c->render = 1;
    c->dirty = 1;
    c->ctx = ctx;
    c->data = gv;

    gv->columns = data;
    gv->row_count = rows;
    gv->col_count = cols;
    gv->selected = rows > 1 ? 1 : -1;
    gv->row_height = c->font_size + c->font_size / 2;
    gv->on_select = on_select;

    build_font_cache(&c->font, c->font_size, &c->font_cache);

    c->on_clear = gridview_on_clear;
    c->on_enable = gridview_on_enable;
    c->on_disable = gridview_on_disable;
    c->on_destroy = gridview_on_destroy;

    if (ui_add_component(ctx, c)) {
        if(c->on_destroy) 
            c->on_destroy(c);

        free(c);
        return NULL;
    }
    return c;
}
