#include "include/button.h"
#include <stdlib.h>
#include <string.h>

// BUTTON
int draw_button(component_t* c, window_t* win) {
	if (!c || !c->visible || !c->render) return 0;
    button_data_t* b = (button_data_t*) c->data;

    ui_lock(&win->lock);

	// Botão com borda arredondada
	draw_rect_component(win->buffer, win->width, win->height,
                    c->x, c->y, c->w, c->h,
                    c->bg,   // cor de fundo (cinza claro)
                    c->border_color,   // borda preta
                    b->border_thickness,            // espessura borda
                    b->border_radius);           // raio do canto

	// Texto centralizado
	if(b->label[0] != '\0'){
        int y = (c->h/2) - (c->font_size/2);
	    draw_text_centered(c->font, b->label, c->x,c->y + y ,c->w, c->fg,
            c->font_size, win);
    }
    
    c->render--;

    ui_unlock(&win->lock);

    return 1;
}

void button_event(component_t* c, event_t ev) {
    button_data_t* b = (button_data_t*) c->data;

    int inside = (ev.mouse.x >= c->x && ev.mouse.x < c->x + c->w &&
                  ev.mouse.y >= c->y && ev.mouse.y < c->y + c->h);

    switch (ev.type) {
        case EVENT_MOUSE_MOVE:
            if (inside) {
                if (!b->hovered) {
                    b->hovered = 1;
                    c->bg = b->bg_hover; // cor hover (mais clara)
                    c->render = 1;      // forçar redesenho
                    c->ctx->window->invalid = 1;
                }
            } else if (b->hovered) {
                b->hovered = 0;
                c->bg = b->bg_normal;     // cor normal
                c->render = 1;
                c->ctx->window->invalid = 1;
            }
            break;

        case EVENT_MOUSE_LEFT_DOWN:
            if (inside) {
                b->pressed = 1;
                c->bg = b->bg_pressed ; // cor de clique
                c->render = 1;
                c->ctx->window->invalid = 1;
            }
            break;

        case EVENT_MOUSE_UP:
            if (b->pressed) {
                b->pressed = 0;
                c->bg = b->hovered ? b->bg_hover : b->bg_normal;
                c->render = 1;
                c->ctx->window->invalid = 1;
                if (inside && b->on_click) {
                    b->on_click(); // dispara callback
                }
            }
            break;
		default:
			break;
    }
}

component_t* create_button(ui_context_t* ctx, int x, int y, int w, int h, const char* label, void (*on_click)()) {
    if (ctx->count >= 64) return NULL;

    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_BUTTON;
    comp->visible = 1;
    comp->on_draw = draw_button;
	comp->on_event = button_event;
	comp->font = *(stbtt_fontinfo *)ctx->window->font;
	comp->font_size = 18;
	comp->fg = 0;
	comp->bg = 0xFFDDDDDD;
	comp->border_color = 0x222222;
	comp->render = 1;

    button_data_t* btn = malloc(sizeof(button_data_t));
    if(label) {
        strncpy(btn->label, label, sizeof(btn->label));
    }else {
        btn->label[0] = '\0';
    }
    
    btn->pressed = 0;
    btn->bg_normal = 0xFFDDDDDD;     // cor normal
    btn->bg_hover = 0xFFEEEEEE; // cor hover (mais clara)
    btn->bg_pressed = 0xFFAAAAAA; // cor de clique
    btn->border_radius = 8;
    btn->border_thickness = 1;
    btn->on_click = on_click;
    comp->data = btn;

    comp->ctx = ctx;
    ctx->components[ctx->count++] = comp;
    return comp;
}

void button_set_label(component_t* c, const char* new_label) {
    if (!c || c->type != UI_BUTTON || !c->data) return;

    button_data_t* b = (button_data_t*)c->data;

    if (!new_label) {
        b->label[0] = '\0';
    } else {
        // Copia garantindo terminação nula
        strncpy(b->label, new_label, sizeof(b->label) - 1);
        b->label[sizeof(b->label) - 1] = '\0';
    }

    // Força redesenho
    c->render = 1;
    if (c->ctx && c->ctx->window)
        c->ctx->window->invalid = 1;
}