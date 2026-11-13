#include "include/components.h"
#include "include/gridview.h"
#include <stdlib.h>
#include <string.h>

// ------------------------------
// Desenha o GridView com cabeçalho
// ------------------------------
int draw_gridview(component_t* c, window_t* win) {
    if (!c || !c->visible || !c->render) return 0;

    gridview_data_t* gv = (gridview_data_t*)c->data;

    ui_lock(&win->lock);
    ui_lock(&gv->spinlock);

    int visible_rows = c->h / gv->row_height - 1; // -1 para cabeçalho
    if (visible_rows <= 0) visible_rows = 1;

    fill_rect(win->buffer, win->width, win->height, c->x, c->y, c->w, c->h, c->bg);

    if (gv->row_count <= 1) { // apenas cabeçalho
        draw_rect_border(win->buffer, win->width, win->height, c->x, c->y, c->w, c->h, 0x000000);
        c->render--;
        ui_unlock(&gv->spinlock);
        ui_unlock(&win->lock);
        return 1;
    }

    int col_width = c->w / gv->col_count;

    // --- cabeçalho fixo ---
    fill_rect(win->buffer, win->width, win->height, c->x, c->y, c->w, gv->row_height, 0xFFD0D0D0);
    draw_rect_border(win->buffer, win->width, win->height, c->x, c->y, c->w, gv->row_height, 0x000000);
    for (int col = 0; col < gv->col_count; col++) {
        draw_text_cached(&c->font_cache, win->buffer, win->width, win->height,
                         gv->columns[0][col], c->x + col*col_width + 4, c->y + 2, col_width-4, c->fg);
    }

    // --- linhas de dados ---
    for (int i = 1; i < gv->row_count; i++) {
        int row_idx = i - 1 - gv->scroll_offset;
        if (row_idx < 0 || row_idx >= visible_rows) continue;

        int yy = c->y + gv->row_height + row_idx * gv->row_height;
        unsigned int bg = (i % 2 == 0) ? 0xFFFFFFFF : 0xFFF8F8F8;

        if (i == gv->selected) {
            bg = 0xFFCCE5FF;
            fill_rect(win->buffer, win->width, win->height, c->x, yy, c->w-8, gv->row_height, bg);
            draw_rect_border(win->buffer, win->width, win->height, c->x, yy, c->w-8, gv->row_height, 0xFF3366FF);
        } else {
            fill_rect(win->buffer, win->width, win->height, c->x, yy, c->w-8, gv->row_height, bg);
        }

        for (int col = 0; col < gv->col_count; col++) {
            draw_text_cached(&c->font_cache, win->buffer, win->width, win->height,
                             gv->columns[i][col], c->x + col*col_width + 4, yy + 2, col_width-4, c->fg);
        }
    }

    // --- scrollbar ---
    int data_rows = gv->row_count - 1;
    if (data_rows > visible_rows) {
        int bar_h = (c->h * visible_rows) / data_rows;
        if (bar_h < 8) bar_h = 8;
        int range = data_rows - visible_rows;
        int bar_y = c->y + gv->row_height + (c->h - gv->row_height - bar_h) * gv->scroll_offset / range;

        fill_rect(win->buffer, win->width, win->height, c->x + c->w - 8, c->y + gv->row_height, 8, c->h - gv->row_height, 0xFFEFEFEF);
        draw_rect_border(win->buffer, win->width, win->height, c->x + c->w - 8, c->y + gv->row_height, 8, c->h - gv->row_height, 0xFF999999);

        fill_rect(win->buffer, win->width, win->height, c->x + c->w - 8, bar_y, 8, bar_h, 0xFFBBBBBB);
        draw_rect_border(win->buffer, win->width, win->height, c->x + c->w - 8, bar_y, 8, bar_h, 0xFF555555);
    }

    c->render--;
    ui_unlock(&gv->spinlock);
    ui_unlock(&win->lock);

    return 1;
}

// ------------------------------
// Evento do GridView com setas
// ------------------------------
void gridview_event(component_t* c, event_t ev) {
    gridview_data_t* gv = (gridview_data_t*)c->data;
    int visible_rows = c->h / gv->row_height - 1; // -1 para cabeçalho

    int inside = (ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w &&
                  ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h);

    // Foco no GridView
    if (ev.type == EVENT_MOUSE_LEFT_DOWN) {
        gv->focused = inside ? 1 : 0;
    }
    if (!gv->focused) return;

    switch (ev.type) {
        case EVENT_MOUSE_LEFT_DOWN: {
            // clique em linha de dados
            if (ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w - 8 &&
                ev.mouse.y >= c->y + gv->row_height && ev.mouse.y < c->y + c->h) {

                int row_idx = (ev.mouse.y - c->y - gv->row_height) / gv->row_height + gv->scroll_offset + 1;
                if (row_idx >= 1 && row_idx < gv->row_count) {
                    gv->selected = row_idx;
                    if (gv->on_select)
                        gv->on_select(row_idx, gv->columns[row_idx]);
                    c->render = 1;
                    c->ctx->window->invalid = 1;
                }
            }
        } break;

        case EVENT_MOUSE_MOVE:
            if (gv->dragging_scroll) {
                int data_rows = gv->row_count - 1;
                int bar_h = (c->h * visible_rows) / data_rows;
                if (bar_h < 8) bar_h = 8;

                int new_bar_y = ev.mouse.y - gv->drag_offset_y;
                if (new_bar_y < c->y + gv->row_height) new_bar_y = c->y + gv->row_height;
                if (new_bar_y > c->y + c->h - bar_h) new_bar_y = c->y + c->h - bar_h;

                gv->scroll_offset = (new_bar_y - c->y - gv->row_height) * (data_rows - visible_rows) / (c->h - gv->row_height - bar_h);
                c->render = 1;
                c->ctx->window->invalid = 1;
            }
            break;

        case EVENT_MOUSE_UP:
            gv->dragging_scroll = 0;
            break;

        case EVENT_KEY_DOWN: {
            // navegação com setas
            if (ev.key.keycode == KEY_DOWN) {
                if (gv->selected < gv->row_count - 1) {
                    gv->selected++;
                    // ajusta scroll se selecionado sair da área visível
                    if (gv->selected - 1 >= gv->scroll_offset + visible_rows) {
                        gv->scroll_offset++;
                    }
                    if (gv->on_select)
                        gv->on_select(gv->selected, gv->columns[gv->selected]);
                    c->render = 1;
                    c->ctx->window->invalid = 1;
                }
            } else if (ev.key.keycode == KEY_UP) {
                if (gv->selected > 1) { // não seleciona cabeçalho
                    gv->selected--;
                    if (gv->selected - 1 < gv->scroll_offset) {
                        gv->scroll_offset--;
                    }
                    if (gv->on_select)
                        gv->on_select(gv->selected, gv->columns[gv->selected]);
                    c->render = 1;
                    c->ctx->window->invalid = 1;
                }
            }
        } break;

        default:
            break;
    }
}

// ------------------------------
// Atualiza os itens do GridView
// ------------------------------
void gridview_set_items(component_t* c, char*** new_data, int rows, int cols) {
    if (!c || !c->data) return;

    gridview_data_t* gv = (gridview_data_t*)c->data;

    ui_lock(&gv->spinlock);

    gv->columns = new_data;
    gv->row_count = rows;
    gv->col_count = cols;

    gv->selected = (rows > 1) ? 1 : -1;
    gv->scroll_offset = 0;

    // Forçar redraw
    c->render = 1;
    if (c->ctx && c->ctx->window)
        c->ctx->window->invalid = 1;

    ui_unlock(&gv->spinlock);
}

// ------------------------------
// Cria GridView
// ------------------------------
component_t* create_gridview(ui_context_t* ctx, int x, int y, int w, int h,
                             char*** data, int rows, int cols,
                             void (*on_select)(int, char**)) {
    if (ctx->count >= 64) return NULL;

    component_t* c = malloc(sizeof(component_t));
    memset(c, 0, sizeof(component_t));

    c->x = x; c->y = y; c->w = w; c->h = h;
    c->type = UI_LISTBOX; // ou UI_GRIDVIEW
    c->visible = 1;
    c->on_draw = draw_gridview;
    c->on_event = gridview_event;
    c->font = *(stbtt_fontinfo *)ctx->window->font;
    c->font_size = 16;
    c->fg = 0x000000;
    c->bg = 0xFFFFFFFF;
    c->border_color = 0x333333;
    c->render = 1;

    gridview_data_t* gv = malloc(sizeof(gridview_data_t));
    memset(gv, 0, sizeof(gridview_data_t));
    gv->columns = data;
    gv->row_count = rows;
    gv->col_count = cols;
    gv->selected = -1;
    gv->focused = 0;
    gv->row_height = c->font_size + (c->font_size/2);
    gv->scroll_offset = 0;
    gv->dragging_scroll = 0;
    gv->on_select = on_select;

    c->data = gv;

    build_font_cache(&c->font, c->font_size, &c->font_cache);

    c->ctx = ctx;
    ctx->components[ctx->count++] = c;

    return c;
}