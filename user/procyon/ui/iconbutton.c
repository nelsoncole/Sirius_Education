#include "include/components.h"
#include "include/iconbutton.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void iconbutton_invalidate(component_t *c)
{
    if (!c) return;

    c->render = 1;
    c->dirty = 1;
}

static int iconbutton_contains(const component_t *c, int x, int y)
{
    if (!c) return 0;

    return x >= c->x && x < c->x + c->w &&
           y >= c->y && y < c->y + c->h;
}

static void iconbutton_on_clear(component_t *c)
{
    if (!c || c->type != UI_BUTTON || !c->data) return;

    iconbutton_data_t *b = c->data;

    b->pressed = 0;
    b->hovered = 0;
    c->bg = b->bg_normal;

    iconbutton_invalidate(c);
}

static void iconbutton_on_enable(component_t *c)
{
    if (!c || c->type != UI_BUTTON || !c->data) return;

    iconbutton_data_t *b = c->data;

    b->pressed = 0;
    b->hovered = 0;
    c->bg = b->bg_normal;

    iconbutton_invalidate(c);
}

static void iconbutton_on_disable(component_t *c)
{
    if (!c || c->type != UI_BUTTON || !c->data) return;

    iconbutton_data_t *b = c->data;

    b->pressed = 0;
    b->hovered = 0;
    c->bg = b->bg_normal;

    iconbutton_invalidate(c);
}

static void iconbutton_on_destroy(component_t *c)
{
    if (!c) return;

    iconbutton_data_t *b = c->data;
    if (!b) return;

    if (b->icon.image) {
        free(b->icon.image);
        b->icon.image = NULL;
    }

    free(b);
    c->data = NULL;
}

int draw_iconbutton(component_t *c, window_t *win)
{
    if (!c || !win || !win->buffer || !c->visible || !c->enabled || !c->render)
        return 0;

    iconbutton_data_t *b = c->data;
    if (!b) return 0;

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

    int icon_x = b->spacing + c->x + (c->w - b->icon_size) / 2;
    int icon_y = c->y + (c->h - b->icon_size) / 2;

    int text_x = c->x;
    int text_y = c->y;
    int text_width = c->w;
    int label_max_height = c->h;

    int text_height = c->font_size;
    int label_width = 0;

    if (b->label[0]) {
        label_width = text_get_string_width(
            &c->font,
            c->font_size,
            b->label,
            strlen(b->label)
        );
    }

    switch (b->layout) {
        case ICON_LABEL_BOTTOM:
            icon_x = c->x + (c->w - b->icon.width) / 2;
            icon_y = c->y + 5;
            text_y = icon_y + b->icon.height + 5;
            text_x = c->x;
            text_width = c->w;
            label_max_height = c->h - (text_y - c->y);
            break;

        case ICON_LABEL_TOP:
            text_y = c->y + 5;
            text_x = c->x + (c->w - label_width) / 2;
            text_width = label_width;
            icon_x = c->x + (c->w - b->icon.width) / 2;
            icon_y = text_y + text_height + 5;
            label_max_height = text_height;
            break;

        case ICON_LABEL_LEFT:
            text_x = c->x + 5;
            text_y = c->y + (c->h - text_height) / 2;
            text_width = label_width;
            icon_x = b->spacing + c->x + c->w - b->icon.width - 5;
            icon_y = c->y + (c->h - b->icon.height) / 2;
            label_max_height = c->h;
            break;

        case ICON_LABEL_RIGHT:
            icon_x = b->spacing + c->x + 5;
            icon_y = c->y + (c->h - b->icon.height) / 2;
            text_x = icon_x + b->icon.width + 5;
            text_y = c->y + (c->h - text_height) / 2;
            text_width = c->w - b->icon.width - 15;
            label_max_height = c->h;
            break;

        case ICON_CENTER:
            icon_x = c->x + (c->w - b->icon.width) / 2;
            icon_y = c->y + (c->h - b->icon.height) / 2;
            break;

        default:
            break;
    }

    if((b->layout == ICON_LABEL_BOTTOM || b->layout ==ICON_LABEL_TOP) && !b->show_label) {
        icon_y = c->y + (c->h-b->icon.height)/2;
    }

    if (b->show_icon && b->icon.image && b->icon.width > 0 && b->icon.height > 0) {
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

    if (b->show_label && b->label[0] && b->layout != ICON_CENTER &&
        text_width > 0 && label_max_height > 0) {
        
        if(b->layout == ICON_LABEL_BOTTOM || b->layout == ICON_LABEL_TOP)
            draw_text_centered_wrapped(
                c->font,
                b->label,
                text_x,
                text_y,
                text_width,
                label_max_height,
                c->fg,
                c->font_size,
                win
            );
        else
             draw_text_wrapped(
                c->font,
                b->label,
                text_x,
                text_y,
                text_width,
                label_max_height,
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

int iconbutton_event(component_t *c, event_t ev)
{
    if (!c || !c->data || !c->enabled || !c->visible)
        return 0;

    iconbutton_data_t *b = c->data;
    int inside = iconbutton_contains(c, ev.mouse.x, ev.mouse.y);
    void (*callback)(component_t *) = NULL;

    switch (ev.type) {
        case EVENT_MOUSE_MOVE:
            if (inside) {
                if (!b->hovered) {
                    b->hovered = 1;
                    if (!b->pressed)
                        c->bg = b->bg_hover;
                    iconbutton_invalidate(c);
                }
            } else if (b->hovered) {
                b->hovered = 0;
                if (!b->pressed)
                    c->bg = b->bg_normal;
                iconbutton_invalidate(c);
            }
            break;

        case EVENT_MOUSE_LEFT_DOWN:
            if (inside) {
                b->pressed = 1;
                c->bg = b->bg_pressed;
                iconbutton_invalidate(c);
            }
            break;

        case EVENT_MOUSE_UP:
            if (b->pressed) {
                b->pressed = 0;
                c->bg = b->hovered && inside ? b->bg_hover : b->bg_normal;
                iconbutton_invalidate(c);

                if (inside)
                    callback = b->on_click;
            }
            break;

        default:
            break;
    }

    if (callback)
        callback(c);

    return c->render;
}

component_t *create_iconbutton(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    const char *label,
    const char *icon_path,
    int icon_size,
    void (*on_click)(component_t *comp))
{
    if (!ctx || !ctx->window || !ctx->window->font ||
        w <= 0 || h <= 0 || icon_size < 0)
        return NULL;

    component_t *comp = calloc(1, sizeof(component_t));
    if (!comp) return NULL;

    iconbutton_data_t *btn = calloc(1, sizeof(iconbutton_data_t));
    if (!btn) {
        free(comp);
        return NULL;
    }

    comp->x = x;
    comp->y = y;
    comp->w = w;
    comp->h = h;
    comp->type = UI_BUTTON;
    comp->visible = 1;
    comp->enabled = 1;
    comp->on_draw = draw_iconbutton;
    comp->on_event = iconbutton_event;
    comp->font = *(stbtt_fontinfo *)ctx->window->font;
    comp->font_size = 16;
    comp->fg = 0xFF000000;
    comp->bg = 0xFFDDDDDD;
    comp->border_color = 0xFF222222;
    comp->render = 1;
    comp->dirty = 1;
    comp->ctx = ctx;
    comp->data = btn;

    if (label) {
        strncpy(btn->label, label, sizeof(btn->label) - 1);
        btn->label[sizeof(btn->label) - 1] = '\0';
        btn->show_label = btn->label[0] != '\0';
    }

    btn->bg_normal = 0xFFDDDDDD;
    btn->bg_hover = 0xFFEEEEEE;
    btn->bg_pressed = 0xFFAAAAAA;
    btn->layout = ICON_LABEL_BOTTOM;
    btn->border_radius = 8;
    btn->border_thickness = 1;
    btn->spacing = 0;
    btn->on_click = on_click;
    btn->icon_size = icon_size;
    btn->icon.width = icon_size;
    btn->icon.height = icon_size;

    if (icon_path && icon_path[0] && icon_size > 0) {
        image_t img;
        memset(&img, 0, sizeof(img));

        if (load_image(icon_path, &img) == 0) {
            if (img.pixels && img.width > 0 && img.height > 0) {
                btn->icon.image = draw_image_resize(
                    img.pixels,
                    img.width,
                    img.height,
                    icon_size,
                    icon_size
                );
            }

            image_free(&img);
            btn->show_icon = btn->icon.image != NULL;
        }
    }

    comp->on_clear = iconbutton_on_clear;
    comp->on_enable = iconbutton_on_enable;
    comp->on_disable = iconbutton_on_disable;
    comp->on_destroy = iconbutton_on_destroy;

    if (ui_add_component(ctx, comp)) {
        if(comp->on_destroy) 
            comp->on_destroy(comp);

        free(comp);
        return NULL;
    }
    
    return comp;
}
