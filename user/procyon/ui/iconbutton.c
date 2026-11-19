#include "include/components.h"
#include "include/iconbutton.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// BUTTON

int draw_iconbutton(component_t* c, window_t* win)
{
    if (!c || !c->visible || !c->render)
        return 0;

    iconbutton_data_t* b = (iconbutton_data_t*)c->data;

    ui_lock(&win->lock);

    draw_rect_component(
        win->buffer,
        win->width,
        win->height,
        c->x,
        c->y,
        c->w,
        c->h,
        c->bg,
        c->border_color,
        b->border_thickness,
        b->border_radius
    );

    int icon_x = c->x + (c->w - b->icon_size) / 2;
    int icon_y = c->y + (c->h - b->icon_size) / 2;

    int text_x = c->x;
    int text_y = c->y;
    int text_width = c->w;
    int label_max_height = c->h;

    int text_height = c->font_size;
    int label_width = text_get_string_width(
        &c->font,
        c->font_size,
        b->label,
        strlen(b->label)
    );

    switch (b->layout)
    {
        case ICON_LABEL_BOTTOM:
            icon_y = c->y + 5;
            text_y = icon_y + b->icon_size + 5;
            text_x = c->x;
            text_width = c->w;
            label_max_height = c->h - (5 + b->icon.height + b->border_radius + b->border_thickness);
            break;

        case ICON_LABEL_TOP:
            text_y = c->y + 5;
            text_x = c->x + (c->w - label_width) / 2;
            text_width = label_width;
            icon_y = text_y + text_height + 5;
            label_max_height = c->h - (5 + b->icon.height + b->border_radius + b->border_thickness);
            break;

        case ICON_LABEL_LEFT:
            text_x = c->x + 5;
            text_y = c->y + (c->h - text_height) / 2;
            text_width = label_width;
            icon_x = c->x + c->w - b->icon_size - 5;
            label_max_height = c->h - (b->border_radius + b->border_thickness);
            break;

        case ICON_LABEL_RIGHT:
            icon_x = c->x + 5;
            text_x = icon_x + b->icon_size + 5;
            text_y = c->y + (c->h - text_height) / 2;
            text_width = c->w - b->icon_size - 15;
            label_max_height = c->h - (b->border_radius + b->border_thickness);
            break;

        case ICON_CENTER:
            icon_x = c->x + ((c->w - b->icon.width)/2);
            icon_y = c->y + ((c->h - b->icon.height)/2);
            break;
    }

    if (b->icon.image && b->show_icon)
    {
        draw_image_tint(
            icon_x,
            icon_y,
            win->width,
            win->height,
            b->icon.image,
            b->icon.width,
            b->icon.height,
            win->buffer,
            c->bg
        );
    }

    if (b->label[0] && b->show_label)
    {
        draw_text_centered_wrapped(c->font, b->label, text_x, text_y, text_width, label_max_height, c->fg, c->font_size, win);
    }

    c->render = 0;

    ui_unlock(&win->lock);

    return 1;
}

int iconbutton_event(component_t* c, event_t ev) {
    iconbutton_data_t* b = (iconbutton_data_t*) c->data;

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
                    b->on_click(c); // dispara callback
                }
            }
            break;
		default:
			break;
    }

    return c->render;
}

component_t* create_iconbutton(ui_context_t* ctx, int x, int y, int w, int h,
    const char* label, const char* icon_path, int icon_size, void (*on_click)(component_t *comp)) {
    if (!ctx)return NULL;

    component_t* comp = malloc(sizeof(component_t));
    memset(comp, 0, sizeof(component_t));

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_BUTTON;
    comp->visible = 1;
    comp->on_draw = draw_iconbutton;
	comp->on_event = iconbutton_event;
	comp->font = *(stbtt_fontinfo *)ctx->window->font;
	comp->font_size = 16;
	comp->fg = 0xFF000000;
	comp->bg = 0xFFDDDDDD;
	comp->border_color = 0xFF222222;
	comp->render = 1;

    iconbutton_data_t* btn = calloc(1, sizeof(iconbutton_data_t));
    if(label) {
        strncpy(btn->label, label, sizeof(btn->label));
        btn->show_label = 1;
    }else {
        btn->label[0] = '\0';
        btn->show_label = 0;
    }
    btn->pressed = 0;
    btn->hovered = 0;
    btn->bg_normal = 0xFFDDDDDD;     // cor normal
    btn->bg_hover = 0xFFEEEEEE; // cor hover (mais clara)
    btn->bg_pressed = 0xFFAAAAAA; // cor de clique
    btn->layout = ICON_LABEL_BOTTOM;
    btn->border_radius = 8;
    btn->border_thickness = 1;
    btn->spacing = 8;
    btn->on_click = on_click;
    btn->icon_size = icon_size;
    btn->icon.width = btn->icon_size;
    btn->icon.height = btn->icon_size;
    btn->icon.image = NULL;
    if(icon_path && icon_size) {
        image_t img;
        if(!load_image(icon_path, &img)) {
            btn->icon.image = draw_image_resize(img.pixels, img.width, img.height, btn->icon.width, btn->icon.height);
            image_free(&img);
            btn->show_icon = 1;
        }else btn->show_icon = 0;
    }

    comp->data = btn;
    comp->ctx = ctx;
    ui_add_component(ctx, comp);
    return comp;
}