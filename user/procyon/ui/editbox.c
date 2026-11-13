#include "include/components.h"
#include "include/editbox.h"
#include <stdlib.h>
#include <string.h>
/*
    Para campo de senha: 
        e->hide_input = 1;
    Para texto multi-linha com scroll:
        e->multiline = 1;
*/

int draw_editbox(component_t* c, window_t* win) {
    if (!c || !c->visible || !c->render) return 0;

    editbox_data_t* e = (editbox_data_t*)c->data;
    ui_lock(&win->lock);
    ui_lock(&e->spinlock);
    // Caixa com borda arredondada
    draw_rect_component(win->buffer, win->width, win->height,
                        c->x, c->y, c->w, c->h,
                        c->bg, c->border_color,
                        e->border_thickness,
                        e->border_radius);

    // Posição base do texto (verticalmente centralizado)
    int text_y = c->y + (c->h - c->font_size) / 2;

    // Texto
    //draw_text(c->font, e->text, c->x + 6, text_y, c->w-12, c->fg, c->font_size, win, win->bankbuffer);
    draw_text_cached(&c->font_cache, win->buffer, win->width, win->height,
        e->text, c->x + 6, text_y, c->w-12, c->fg);
    // Cursor (se estiver focado)
    if (e->focused) {
        int text_w = text_get_string_width(&c->font, c->font_size, e->text, e->cursor_pos);
        int cx = c->x + 6 + text_w;
        int cy1 = text_y;                 // início do cursor = topo do texto
        int cy2 = text_y + c->font_size;  // fim do cursor = altura da fonte

        // Limitar o cursor dentro da caixa
        if (cx > c->x + c->w - 10) cx = c->x + c->w - 10;

        draw_line(win->buffer, win->width, win->height, cx, cy1, cx, cy2, c->fg);
    }

    c->render--;
    ui_unlock(&e->spinlock);
    ui_unlock(&win->lock);

    return 1;
}

static void editbox_insert_char(editbox_data_t* e, char ch) {
    ui_lock(&e->spinlock);
    size_t len = strlen(e->text);

    if (len >= sizeof(e->text) - 1){
        ui_unlock(&e->spinlock);
        return; // cheio
    } 


    // move caracteres para frente
    memmove(&e->text[e->cursor_pos + 1],
            &e->text[e->cursor_pos],
            len - e->cursor_pos + 1);

    e->text[e->cursor_pos] = ch;
    e->cursor_pos++;

    ui_unlock(&e->spinlock);
}

static void editbox_delete_char(editbox_data_t* e, int backspace) {
    ui_lock(&e->spinlock);
    size_t len = strlen(e->text);

    if (backspace) {
        if (e->cursor_pos > 0) {
            memmove(&e->text[e->cursor_pos - 1],
                    &e->text[e->cursor_pos],
                    len - e->cursor_pos + 1);
            e->cursor_pos--;
        }
    } else {
        if (e->cursor_pos < (int)len) {
            memmove(&e->text[e->cursor_pos],
                    &e->text[e->cursor_pos + 1],
                    len - e->cursor_pos);
        }
    }

    ui_unlock(&e->spinlock);
}

void editbox_event(component_t* c, event_t ev) {
    editbox_data_t* e = (editbox_data_t*)c->data;

    int inside = (ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w &&
                  ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h);

    switch (ev.type) {
        case EVENT_MOUSE_LEFT_DOWN:
			if (inside) { 
				e->focused = 1;
            	c->render = 1;
                c->ctx->window->invalid = 1;
			}else {
				if(e->focused){
                    c->render = 1;
                    c->ctx->window->invalid = 1;
                }
				e->focused = 0;
			}
            break;

        case EVENT_KEY_DOWN:
            if (!e->focused) break;

            switch (ev.key.keycode) {
                case KEY_BACKSPACE:
                    editbox_delete_char(e, 1);
                    c->render = 1;
                    c->ctx->window->invalid = 1;
                    break;
                case KEY_DELETE:
                    editbox_delete_char(e, 0);
                    c->render = 1;
                    c->ctx->window->invalid = 1;
                    break;
                case KEY_LEFT:
                    if (e->cursor_pos > 0) {
                        e->cursor_pos--;
                        c->render = 1;
                        c->ctx->window->invalid = 1;
                    }
                    break;
                case KEY_RIGHT:
                    if (e->cursor_pos < (int)strlen(e->text)) {
                        e->cursor_pos++;
                        c->render = 1;
                        c->ctx->window->invalid = 1;
                    }
                    break;
                default:
                    break;
            }
            break;

        case EVENT_TEXT_INPUT:
            if (!e->focused) break;
            editbox_insert_char(e, (char)ev.text.codepoint);
            c->render = 1;
            c->ctx->window->invalid = 1;
            break;

        default:
            break;
    }
}
component_t* create_editbox(ui_context_t* ctx, int x, int y, int w, int h) {
    if (ctx->count >= 64) return NULL;

    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_EDITTEXT;
    comp->visible = 1;
    comp->on_draw = draw_editbox;
	comp->on_event = editbox_event;
    comp->font = *(stbtt_fontinfo *)ctx->window->font;
    comp->font_size = 16;
    comp->fg = 0x000000;
    comp->bg = 0xFFFFFF;
    comp->border_color = 0x444444;
	comp->render = 1;

    editbox_data_t* e = malloc(sizeof(editbox_data_t));
    memset(e->text, 0, sizeof(e->text));
    e->cursor_pos = 0;
    e->focused = 0;
    e->border_radius = 6;
    e->border_thickness = 1;
    comp->data = e;

    // --- inicializar cache da fonte aqui ---
    build_font_cache(&comp->font, comp->font_size, &comp->font_cache);

    comp->ctx = ctx;
    ctx->components[ctx->count++] = comp;
    return comp;
}


void editbox_set_text(component_t* c, const char* text) {
    if (!c || !text) return;

    editbox_data_t* e = (editbox_data_t*)c->data;
    if (!e) return;

    ui_lock(&e->spinlock);

    // Copiar texto com segurança
    strncpy(e->text, text, sizeof(e->text) - 1);
    e->text[sizeof(e->text) - 1] = '\0';

    // Ajustar cursor para o final do texto
    e->cursor_pos = strlen(e->text);

    ui_unlock(&e->spinlock);

    // Marcar para redesenhar
    c->render = 1;
    if (c->ctx && c->ctx->window)
        c->ctx->window->invalid = 1;
}

void editbox_set_text_ex(component_t* c, const char* text, int cursor_pos) {
    if (!c || !text) return;

    editbox_data_t* e = (editbox_data_t*)c->data;
    if (!e) return;

    ui_lock(&e->spinlock);

    strncpy(e->text, text, sizeof(e->text) - 1);
    e->text[sizeof(e->text) - 1] = '\0';

    int len = strlen(e->text);

    // Clamp do cursor
    if (cursor_pos < 0) cursor_pos = 0;
    if (cursor_pos > len) cursor_pos = len;

    e->cursor_pos = cursor_pos;

    ui_unlock(&e->spinlock);

    c->render = 1;
    if (c->ctx && c->ctx->window)
        c->ctx->window->invalid = 1;
}