#include "include/components.h"
#include "include/listbox.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// LISTBOX
// ---- Desenha o listbox ----
int draw_listbox(component_t* c, window_t* win) {
    if (!c || !c->visible || !c->render) return 0;

    listbox_data_t* lb = (listbox_data_t*)c->data;

    ui_lock(&win->lock);
    ui_lock(&lb->spinlock);

    int visible_rows = c->h / lb->item_height;
    if (visible_rows <= 0) visible_rows = 1;

    // fundo geral
    fill_rect(win->buffer, win->width, win->height, c->x, c->y, c->w, c->h, c->bg);

    if (lb->count == 0) {
        draw_rect_border(win->buffer, win->width, win->height,
                         c->x, c->y, c->w, c->h, 0x000000);
        c->render--;
        ui_unlock(&lb->spinlock);
        ui_unlock(&win->lock);
        return 1;
    }

    // corrige scroll_offset
    if (lb->scroll_offset < 0) lb->scroll_offset = 0;
    if (lb->scroll_offset > lb->count - visible_rows)
        lb->scroll_offset = (lb->count > visible_rows) ? lb->count - visible_rows : 0;

    // desenha itens visíveis
    for (int i = 0; i < visible_rows && (i + lb->scroll_offset) < lb->count; i++) {
        int idx = i + lb->scroll_offset;
        int yy = c->y + i * lb->item_height;

        // fundo alternado
        // unsigned int bg = (i % 2 == 0) ? 0xFFF8F8F8 : c->bg;
        // sem alternar fundo
        unsigned int bg = c->bg;

        // item selecionado
        if (idx == lb->selected) {
            bg = 0xFFCCE5FF; // azul clarinho
            fill_rect(win->buffer, win->width, win->height, c->x, yy, c->w - 8, lb->item_height, bg);
            draw_rect_border(win->buffer, win->width, win->height,
                             c->x, yy, c->w - 8, lb->item_height, 0xFF3366FF);
        } else {
            fill_rect(win->buffer, win->width, win->height, c->x, yy, c->w - 8, lb->item_height, bg);
        }

        // texto
        /*draw_text(c->font, lb->items[idx], c->x + 4, yy + 2,
                  c->w - 12, c->fg, c->font_size, win);*/
        draw_text_cached(&c->font_cache, win->buffer, win->width, win->height,
        lb->items[idx], c->x + 4, yy + 2, c->w - 12, c->fg);

    }

    // borda externa
    draw_rect_border(win->buffer, win->width, win->height,
                     c->x, c->y, c->w, c->h, 0x000000);

    // scrollbar
    if (lb->count > visible_rows) {
        int bar_h = (c->h * visible_rows) / lb->count;
        if (bar_h < 8) bar_h = 8;
        int range = lb->count - visible_rows;
        int bar_y = c->y + (c->h - bar_h) * lb->scroll_offset / range;

        // trilho
        fill_rect(win->buffer, win->width, win->height, c->x + c->w - 8, c->y, 8, c->h, 0xFFEFEFEF);
        draw_rect_border(win->buffer, win->width, win->height,
                         c->x + c->w - 8, c->y, 8, c->h, 0xFF999999);

        // thumb
        fill_rect(win->buffer, win->width, win->height, c->x + c->w - 8, bar_y, 8, bar_h, 0xFFBBBBBB);
        draw_rect_border(win->buffer, win->width, win->height,
                         c->x + c->w - 8, bar_y, 8, bar_h, 0xFF555555);
    }

    c->render--;
    ui_unlock(&lb->spinlock);
    ui_unlock(&win->lock);

    return 1;
}

// ---- Eventos do listbox ----
void listbox_event(component_t* c, event_t ev) {
    listbox_data_t* lb = (listbox_data_t*)c->data;

    int visible_rows = c->h / lb->item_height;
    if (visible_rows <= 0) visible_rows = 1;

    int inside = (ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w &&
                  ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h);

    if (ev.type == EVENT_MOUSE_LEFT_DOWN) {
			if (inside) { 
				lb->focused = 1;
			}else {
				if(lb->focused){
                    c->render = 1;
                    c->ctx->window->invalid = 1;
                }
				lb->focused = 0;
			}
    }
    
    if(!lb->focused) return;

    switch (ev.type) {
        case EVENT_MOUSE_LEFT_DOWN: {
        // Scrollbar (apenas se necessário)
        if (lb->count > visible_rows) {

            int bar_h = (c->h * visible_rows) / lb->count;
            if (bar_h < 8) bar_h = 8;
            int bar_y = c->y + (c->h - bar_h) * lb->scroll_offset / (lb->count - visible_rows);

            // clique dentro do thumb
            if (ev.mouse.x >= c->x + c->w - 8 && ev.mouse.x < c->x + c->w &&
                ev.mouse.y >= bar_y && ev.mouse.y < bar_y + bar_h) {
                lb->dragging_scroll = 1;
                lb->drag_offset_y = ev.mouse.y - bar_y;
                break;
            }

            // clique no trilho
            if (ev.mouse.x >= c->x + c->w - 8 && ev.mouse.x < c->x + c->w) {
                int pos = (ev.mouse.y - c->y) * (lb->count - visible_rows) / (c->h - bar_h);
                lb->scroll_offset = pos;

                if (lb->scroll_offset < 0) lb->scroll_offset = 0;
                if (lb->scroll_offset > lb->count - visible_rows)
                    lb->scroll_offset = lb->count - visible_rows;

                c->render = 1;
                c->ctx->window->invalid = 1;
                break;
            }
        }

        // Clique em item (SEMPRE ativo)
        if (ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w - 8 &&
            ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h) {

            int idx = (ev.mouse.y - c->y) / lb->item_height + lb->scroll_offset;

            if (idx >= 0 && idx < lb->count) {
                lb->selected = idx;
                if (lb->on_select)
                    lb->on_select(idx, lb->items[idx]);

                c->render = 1;
                c->ctx->window->invalid = 1;
            }
        }

    } break;

        case EVENT_MOUSE_MOVE:
            if (lb->dragging_scroll) {
                int bar_h = (c->h * visible_rows) / lb->count;
                if (bar_h < 8) bar_h = 8;
                int new_bar_y = ev.mouse.y - lb->drag_offset_y;
                if (new_bar_y < c->y) new_bar_y = c->y;
                if (new_bar_y > c->y + c->h - bar_h) new_bar_y = c->y + c->h - bar_h;

                int pos = (new_bar_y - c->y) * (lb->count - visible_rows) / (c->h - bar_h);
                lb->scroll_offset = pos;
                c->render = 1;
                c->ctx->window->invalid = 1;
            }
            break;

        case EVENT_MOUSE_UP:
            lb->dragging_scroll = 0;
            break;

        case EVENT_KEY_DOWN:
            switch (ev.key.keycode) {
                case KEY_DOWN:
                    if (lb->selected < lb->count - 1) {
                        lb->selected++;
                        if (lb->selected >= lb->scroll_offset + visible_rows) {
                            lb->scroll_offset++;
                        }
                        if (lb->on_select) lb->on_select(lb->selected, lb->items[lb->selected]);
                        c->render = 1;
                        c->ctx->window->invalid = 1;
                    }
                    break;
                case KEY_UP:
                    if (lb->selected > 0) {
                        lb->selected--;
                        if (lb->selected < lb->scroll_offset) {
                            lb->scroll_offset--;
                        }
                        if (lb->on_select) lb->on_select(lb->selected, lb->items[lb->selected]);
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

void listbox_set_items(component_t* c, const char* items[], int count) {
    if (!c || c->type != UI_LISTBOX || !c->data) return;

    listbox_data_t* lb = (listbox_data_t*)c->data;

    // Criar nova lista COMPLETA antes do lock
    char** new_items = NULL;

    if (count > 0) {
        new_items = malloc(sizeof(char*) * count);
        if (!new_items) return;

        for (int i = 0; i < count; i++) {
            size_t len = strlen(items[i]);
            new_items[i] = malloc(len + 1);
            if (!new_items[i]) {
                // cleanup parcial se falhar
                for (int j = 0; j < i; j++)
                    free(new_items[j]);
                free(new_items);
                return;
            }
            memcpy(new_items[i], items[i], len + 1);
        }
    }

    // Trocar ponteiros com lock
    ui_lock(&lb->spinlock);

    char** old_items = lb->items;
    int old_count = lb->count;

    lb->items = new_items;
    lb->count = count;
    lb->selected = (count > 0) ? 0 : -1;
    lb->scroll_offset = 0;

    ui_unlock(&lb->spinlock);

    // Libertar memória antiga FORA do lock
    if (old_items) {
        for (int i = 0; i < old_count; i++) {
            free(old_items[i]);
        }
        free(old_items);
    }

    // Forçar redraw
    c->render = 1;
    if (c->ctx && c->ctx->window)
        c->ctx->window->invalid = 1;
}

component_t* create_listbox(ui_context_t* ctx, int x, int y, int w, int h,
                            const char* items[], int count,
                            void (*on_select)(int, const char*)) {
    if (ctx->count >= 64) return NULL;
    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = x; comp->y = y; comp->w = w; comp->h = h;
    comp->type = UI_LISTBOX;
    comp->visible = 1;
    comp->on_draw = draw_listbox;
    comp->on_event = listbox_event;
    comp->font = *(stbtt_fontinfo *)ctx->window->font;
    comp->font_size = 16;
    comp->fg = 0x000000;
    comp->bg = 0xFFFFFFFF;
    comp->border_color = 0x333333;
    comp->render = 1;

    listbox_data_t* lb = malloc(sizeof(listbox_data_t));
    memset(lb, 0, sizeof(listbox_data_t));
    lb->items = malloc(sizeof(char*) * count);
    lb->count = count;
    for (int i = 0; i < lb->count; i++) {
        
        lb->items[i] = malloc(256);   // aloca espaço para cada string
        if (lb->items[i]) {
            strncpy(lb->items[i], items[i], 255);
            lb->items[i][255] = '\0'; // garante null-terminator
        }else {
            lb->items[i][0] = '\0';
        }
    }
    lb->selected = (lb->count > 0) ? 0 : -1;
    lb->on_select = on_select;
    lb->focused = 0;
    lb->item_height = comp->font_size + (comp->font_size/2);
    lb->scroll_offset = 0;
    lb->dragging_scroll = 0;
    comp->data = lb;

    // --- inicializar cache da fonte aqui ---
    build_font_cache(&comp->font, comp->font_size, &comp->font_cache);

    comp->ctx = ctx;
    ctx->components[ctx->count++] = comp;
    return comp;
}
