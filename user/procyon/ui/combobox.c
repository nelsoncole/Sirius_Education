#include "include/components.h"
#include "include/combobox.h"
#include <stdlib.h>
#include <string.h>

// COMBOBOX
int draw_combobox(component_t* c, window_t* win) {
    if (!c || !c->visible || !c->render) return 0;
    combobox_data_t* cb = (combobox_data_t*)c->data;

    ui_lock(&win->lock);

    // Caixa principal
    fill_rect(win->buffer, win->width, win->height,
              c->x, c->y, c->w, c->h, 0xFFEFEFEF);
    draw_rect_border(win->buffer, win->width, win->height,
                     c->x, c->y, c->w, c->h, 0x000000);

    // Texto do item selecionado
    int yy = c->y + ((c->h - c->font_size) / 2);
    if (cb->count > 0 && cb->selected >= 0) {
        draw_text_cached(&c->font_cache, win->buffer, win->width, win->height,
            cb->items[cb->selected], c->x + 5, yy, c->w - 20, c->fg);
    }

    // Seta ▼
    draw_text_cached(&c->font_cache, win->buffer, win->width, win->height,
        "v", c->x + c->w - 15, yy, c->w - 15, c->fg);

    // Dropdown expandido
    if (cb->expanded && cb->win) {
        ui_lock(&cb->win->lock);

        int visible_rows = cb->win->height / cb->item_height;
        // Fundo
        fill_rect(cb->win->buffer, cb->win->width, cb->win->height,
                  0, 0, cb->win->width, cb->win->height, c->bg);

        // Corrige scroll
        if (cb->scroll_offset < 0) cb->scroll_offset = 0;
        if (cb->scroll_offset > cb->count - visible_rows)
            cb->scroll_offset = (cb->count > visible_rows) ? cb->count - visible_rows : 0;

        for (int i = 0; i < visible_rows && (i + cb->scroll_offset) < cb->count; i++) {
            int idx = i + cb->scroll_offset;
            yy = i * cb->item_height;

            unsigned int bg = c->bg;
            if (idx == cb->selected) {
                bg = 0xFFCCE5FF;
                fill_rect(cb->win->buffer, cb->win->width, cb->win->height,
                          0, yy, c->w - 8, cb->item_height, bg);
                draw_rect_border(cb->win->buffer, cb->win->width, cb->win->height,
                                 0, yy, c->w - 8, cb->item_height, 0xFF3366FF);
            } else {
                fill_rect(cb->win->buffer, cb->win->width, cb->win->height,
                          0, yy, c->w - 8, cb->item_height, bg);
            }

            yy += (cb->item_height - c->font_size) / 2;
            draw_text_cached(&c->font_cache, cb->win->buffer, cb->win->width, cb->win->height,
                cb->items[idx], 5, yy, c->w - 12, c->fg);
        }

        // Scrollbar
        if (cb->count > visible_rows) {
            int bar_h = (cb->win->height * visible_rows) / cb->count;
            if (bar_h < 8) bar_h = 8;
            int bar_y = (cb->win->height - bar_h) * cb->scroll_offset / (cb->count - visible_rows);

            // trilho
            fill_rect(cb->win->buffer, cb->win->width, cb->win->height,
                      c->w - 8, 0, 8, cb->win->height, 0xFFEFEFEF);
            draw_rect_border(cb->win->buffer, cb->win->width, cb->win->height,
                             c->w - 8, 0, 8, cb->win->height, 0xFF999999);

            // thumb
            fill_rect(cb->win->buffer, cb->win->width, cb->win->height,
                      c->w - 8, bar_y, 8, bar_h, 0xFFBBBBBB);
            draw_rect_border(cb->win->buffer, cb->win->width, cb->win->height,
                             c->w - 8, bar_y, 8, bar_h, 0xFF555555);
        }

        // borda externa
        draw_rect_border(cb->win->buffer, cb->win->width, cb->win->height,
            0, 0, cb->win->width, cb->win->height, 0x000000);

        ui_unlock(&cb->win->lock);
    }

    c->render--;

    ui_unlock(&win->lock);

    return 1;
}

void combobox_event(component_t* c, event_t ev) {
    if (!c || !c->visible) return;
    combobox_data_t* cb = (combobox_data_t*)c->data;

    int inside_main = (ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w &&
                       ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h);

    // --- Clique na caixa principal (abre/fecha) ---
    if (ev.type == EVENT_MOUSE_LEFT_DOWN && inside_main) {
        cb->expanded = !cb->expanded;

        if (cb->expanded) {

            c->ctx->modal_active = 1;
            c->ctx->modal_owner = c;  // o dono do modal é esse componente
            // cria overlay
            int x = c->ctx->window->x + c->x;
            int y = c->ctx->window->y + c->y + c->h;
            int w = c->w;
            int h = (cb->count > 0 ? (cb->count * cb->item_height) : c->h);

            // limitar altura (ex: x itens máx visíveis)
            int limit_h = c->ctx->window->height - c->y - c->h - 8; 
            if (h > limit_h) h = (limit_h / cb->item_height) * cb->item_height;

            cb->win = overlay_window_create(x, y, w, h, 0xC0C0C0, 0);
            if (cb->win) {
                enviar_janela_filha_ao_servidor(c->ctx->fd, cb->win, c->ctx->window);
            }
        } else {
            if (cb->win) {
                c->ctx->modal_active = 0;
                c->ctx->modal_owner = NULL;
                enviar_remocao_janela_filha(c->ctx->fd, c->ctx->window);
                window_destroy(cb->win);
                cb->win = NULL;
            }
        }

        c->render = 1;
        c->ctx->window->invalid = 1;
        return;
    }

    // --- Se não está expandido, ignora ---
    if (!cb->expanded || !cb->win) return;

    // --- Lógica de interação dentro do dropdown ---
    int visible_rows = cb->win->height / cb->item_height;
    if (visible_rows <= 0) visible_rows = 1;

    int rel_x = ev.mouse.x - c->x;
    int rel_y = ev.mouse.y - (c->y + c->h); // y relativo ao início do dropdown

    switch (ev.type) {
        case EVENT_MOUSE_LEFT_DOWN: {
            // Scrollbar existe?
            if (cb->count > visible_rows) {
                int bar_h = (cb->win->height * visible_rows) / cb->count;
                if (bar_h < 8) bar_h = 8;
                int bar_y = (cb->win->height - bar_h) * cb->scroll_offset / (cb->count - visible_rows);

                // clique dentro do thumb
                if (rel_x >= c->w - 8 && rel_x < c->w &&
                    rel_y >= bar_y && rel_y < bar_y + bar_h) {
                    cb->dragging_scroll = 1;
                    cb->drag_offset_y = rel_y - bar_y;
                    break;
                }

                // clique no trilho
                if (rel_x >= c->w - 8 && rel_x < c->w) {
                    int pos = (rel_y) * (cb->count - visible_rows) / (cb->win->height - bar_h);
                    cb->scroll_offset = pos;
                    if (cb->scroll_offset < 0) cb->scroll_offset = 0;
                    if (cb->scroll_offset > cb->count - visible_rows)
                        cb->scroll_offset = cb->count - visible_rows;
                    c->render = 1;
                    c->ctx->window->invalid = 1;
                    break;
                }
            }

            // clique em item
            if (rel_x >= 0 && rel_x < c->w - 8 &&
                rel_y >= 0 && rel_y < cb->win->height) {
                int idx = (rel_y / cb->item_height) + cb->scroll_offset;
                if (idx >= 0 && idx < cb->count) {
                    cb->selected = idx;
                    cb->expanded = 0;
                    c->ctx->modal_active = 0;
                    c->ctx->modal_owner = NULL;
                    if (cb->win) {
                        enviar_remocao_janela_filha(c->ctx->fd, c->ctx->window);
                        window_destroy(cb->win);
                        cb->win = NULL;
                    }
                    if (cb->on_select) cb->on_select(idx, cb->items[idx]);
                    c->render = 1;
                    c->ctx->window->invalid = 1;
                }
            } else {
                // clique fora → fecha dropdown
                cb->expanded = 0;
                c->ctx->modal_active = 0;
                c->ctx->modal_owner = NULL;
                if (cb->win) {
                    enviar_remocao_janela_filha(c->ctx->fd, c->ctx->window);
                    window_destroy(cb->win);
                    cb->win = NULL;
                }
                c->render = 1;
                c->ctx->window->invalid = 1;
            }
        } break;

        case EVENT_MOUSE_MOVE:
            if (cb->dragging_scroll) {
                int bar_h = (cb->win->height * visible_rows) / cb->count;
                if (bar_h < 8) bar_h = 8;
                int new_bar_y = rel_y - cb->drag_offset_y;
                if (new_bar_y < 0) new_bar_y = 0;
                if (new_bar_y > cb->win->height - bar_h) new_bar_y = cb->win->height - bar_h;

                int pos = (new_bar_y) * (cb->count - visible_rows) / (cb->win->height - bar_h);
                cb->scroll_offset = pos;
                c->render = 1;
                c->ctx->window->invalid = 1;
            }
            break;

        case EVENT_MOUSE_UP:
            cb->dragging_scroll = 0;
            break;

        case EVENT_KEY_DOWN:
            switch (ev.key.keycode) {
                case KEY_DOWN:
                    if (cb->selected < cb->count - 1) {
                        cb->selected++;
                        if (cb->selected >= cb->scroll_offset + visible_rows) {
                            cb->scroll_offset++;
                        }
                        if (cb->on_select) cb->on_select(cb->selected, cb->items[cb->selected]);
                        c->render = 1;
                        c->ctx->window->invalid = 1;
                    }
                    break;
                case KEY_UP:
                    if (cb->selected > 0) {
                        cb->selected--;
                        if (cb->selected < cb->scroll_offset) {
                            cb->scroll_offset--;
                        }
                        if (cb->on_select) cb->on_select(cb->selected, cb->items[cb->selected]);
                        c->render = 1;
                        c->ctx->window->invalid = 1;
                    }
                    break;
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

component_t* create_combobox(ui_context_t* ctx, int x, int y, int w, int h,
                             const char* items[], int count,
                             void (*on_select)(int, const char*)) {
    if (ctx->count >= 64) return NULL;

    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = x; comp->y = y; comp->w = w; comp->h = h;
    comp->type = UI_COMBOBOX;   // <- define tipo correto
    comp->visible = 1;
    comp->on_draw = draw_combobox;     // função de render
    comp->on_event = combobox_event;   // função de eventos
    comp->font = *(stbtt_fontinfo *)ctx->window->font;
    comp->font_size = (float)(h * 0.8);
    comp->fg = 0x000000;
    comp->bg = 0xFFFFFFFF;
    comp->border_color = 0x333333;
    comp->render = 1;

    combobox_data_t* cb = malloc(sizeof(combobox_data_t));
    memset(cb, 0, sizeof(combobox_data_t));
    cb->items = malloc(sizeof(char*) * count);
    cb->count = count;
    for (int i = 0; i < cb->count; i++) {

        cb->items[i] = malloc(256);   // aloca espaço para cada string
        if (cb->items[i]) {
            strncpy(cb->items[i], items[i], 255);
            cb->items[i][255] = '\0'; // garante null-terminator
        }else {
            cb->items[i][0] = '\0';
        }
    }
    cb->selected = 0;
    cb->expanded = 0;
    cb->on_select = on_select;
    cb->item_height = h;//(int)(comp->font_size + (comp->font_size / 2));
    cb->scroll_offset = 0;
    cb->dragging_scroll = 0;
    cb->win = 0;
    comp->data = cb;

     // --- inicializar cache da fonte aqui ---
    build_font_cache(&comp->font, comp->font_size, &comp->font_cache);

    comp->ctx = ctx;
    ctx->components[ctx->count++] = comp;
    return comp;
}