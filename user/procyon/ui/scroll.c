#include "include/components.h"
#include "include/scroll.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

static int scroll_clamp_offset(scroll_data_t *d, int h)
{
    if (!d)
        return 0;

    int max_offset = d->content_height - h;

    if (max_offset < 0)
        max_offset = 0;

    if (d->scroll_offset < 0)
        d->scroll_offset = 0;

    if (d->scroll_offset > max_offset)
        d->scroll_offset = max_offset;

    return max_offset;
}

static int scroll_contains(component_t *c, int x, int y)
{
    return x >= c->x && y >= c->y &&
           x < c->x + c->w && y < c->y + c->h;
}

component_t *create_scroll(ui_context_t *ctx, int x, int y, int w, int h)
{
    if (!ctx || w <= 0 || h <= 0)
        return NULL;

    component_t *c = calloc(1, sizeof(component_t));

    if (!c)
        return NULL;

    scroll_data_t *d = calloc(1, sizeof(scroll_data_t));

    if (!d) {
        free(c);
        return NULL;
    }

    d->child_capacity = 8;
    d->children = calloc(d->child_capacity, sizeof(component_t *));

    if (!d->children) {
        free(d);
        free(c);
        return NULL;
    }

    d->scrollbar_width = SCROLLBAR_WIDTH;
    d->background_render = 1;
    d->border_radius = 0;
    d->border_thickness = 0;

    d->ctx = malloc(sizeof(ui_context_t));
    if (!d->ctx) {
        free(d->children);
        free(d);
        free(c);
        return NULL;
    }

    memset(d->ctx, 0, sizeof(ui_context_t));

    d->ctx->fd = ctx->fd;
    d->width = w - (d->scrollbar_width+(d->border_thickness*2));
    d->height = h - (d->border_thickness*2);
    d->ctx->window = component_window_create(ctx->window, d->width, d->height, argb(255, 245, 245, 245), 0);

    if (!d->ctx->window)
    {
        free(d->ctx);
        free(d->children);
        free(d);
        free(c);
        return NULL;
    }

    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;
    c->type = UI_SCROLL;
    c->visible = 1;
    c->render = 1;
    c->ctx = ctx;
    c->data = d;
    c->font = *(stbtt_fontinfo *)ctx->window->font;
    c->font_size = 16;
    c->bg = argb(255, 245, 245, 245);
    c->border_color = argb(255, 210, 210, 210);

    c->on_draw = draw_scroll;
    c->on_event = scroll_event;

    if (ui_add_component(ctx, c) != 0) {
        free(d->ctx);
        free(d->children);
        free(d);
        free(c);
        return NULL;
    }

    return c;
}

void scroll_recalculate_layout(component_t *scroll)
{
    if (!scroll || scroll->type != UI_SCROLL)
        return;

    scroll_data_t *d = scroll->data;

    if (!d || d->child_count <= 0)
    {
        if (d)
            scroll_set_content_height(scroll, 0);

        return;
    }

    int content_height = 0;

    switch (d->layout)
    {
        case SCROLL_LAYOUT_VERTICAL:
        {
            int y = 0;

            for (int i = 0; i < d->child_count; i++)
            {
                component_t *child = d->children[i];

                if (!child)
                    continue;

                child->x = 0;
                child->y = y;

                y += child->h + d->spacing_y;
            }

            content_height = y - d->spacing_y;
            break;
        }

        case SCROLL_LAYOUT_HORIZONTAL:
        {
            int x = 0;
            int max_height = 0;

            for (int i = 0; i < d->child_count; i++)
            {
                component_t *child = d->children[i];

                if (!child)
                    continue;

                child->x = x;
                child->y = 0;

                x += child->w + d->spacing_x;

                if (child->h > max_height)
                    max_height = child->h;
            }

            content_height = max_height;
            break;
        }

        case SCROLL_LAYOUT_GRID:
        {
            int cell_width = d->item_width + d->spacing_x;
            int cell_height = d->item_height + d->spacing_y;

            if (cell_width <= 0 || cell_height <= 0)
                return;

            int available_width = d->width;

            int cols = (available_width + d->spacing_x) / cell_width;

            if (cols < 1)
                cols = 1;

            int rows = (d->child_count + cols - 1) / cols;

            for (int i = 0; i < d->child_count; i++)
            {
                component_t *child = d->children[i];

                if (!child)
                    continue;

                int col = i % cols;
                int row = i / cols;

                child->x = col * cell_width;
                child->y = row * cell_height;
            }

            content_height =
                rows * d->item_height +
                (rows - 1) * d->spacing_y;

            break;
        }

        default:
            return;
    }

    for (int i = 0; i < d->child_count; i++)
    {
        if (d->children[i])
            d->children[i]->render = 1;
    }

    scroll_set_content_height(scroll, content_height);

    d->background_render = 1;
    scroll->render = 1;
}

void scroll_set_layout(
    component_t *scroll,
    scroll_layout_t layout,
    int spacing_x,
    int spacing_y,
    int item_width,
    int item_height)
{
    if (!scroll || scroll->type != UI_SCROLL)
        return;

    scroll_data_t *d = scroll->data;

    if (!d)
        return;

    d->layout = layout;
    d->spacing_x = spacing_x;
    d->spacing_y = spacing_y;
    d->item_width = item_width;
    d->item_height = item_height;

    scroll_recalculate_layout(scroll);
}

int scroll_add_component(component_t *scroll, component_t *child)
{
    if (!scroll || !child || scroll->type != UI_SCROLL)
        return -1;

    scroll_data_t *d = scroll->data;

    if (!d || !d->ctx || !d->ctx->window)
        return -1;

    window_t *win = d->ctx->window;

    if (child->y < 0 || child->h <= 0)
        return -1;

    int bottom = child->y + child->h;

    if (bottom < child->y)
        return -1;

    int required_height = bottom;

    if (required_height < d->content_height)
        required_height = d->content_height;

    if (required_height > win->height) {
        
        size_t new_size =
            (size_t)win->width * required_height * sizeof(uint32_t);

        uint32_t *new_buffer = realloc(win->buffer, new_size);

        if (!new_buffer)
            return -1;

        uint32_t *buffer = new_buffer;

        size_t old_pixels =
            (size_t)win->width * win->height;

        size_t new_pixels =
            (size_t)win->width * required_height;

        for (size_t i = old_pixels; i < new_pixels; i++)
            buffer[i] = win->bg;

        win->buffer = new_buffer;
        win->height = required_height;
    }

    if (d->child_count >= d->child_capacity) {

        int new_capacity = d->child_capacity * 2;

        if (new_capacity <= d->child_capacity)
            return -1;

        component_t **new_children = realloc(
            d->children,
            (size_t)new_capacity * sizeof(component_t *)
        );

        if (!new_children)
            return -1;

        d->children = new_children;
        d->child_capacity = new_capacity;
    }

    d->children[d->child_count++] = child;

    scroll_recalculate_layout(scroll);

    d->background_render = 1;
    scroll->render = 1;

    if (scroll->ctx && scroll->ctx->window)
        scroll->ctx->window->invalid = 1;

    return 0;
}

void scroll_set_content_height(component_t *scroll, int content_height)
{
    if (!scroll || scroll->type != UI_SCROLL)
        return;

    scroll_data_t *d = scroll->data;

    if (!d)
        return;

    d->content_height = content_height;

    scroll_clamp_offset(d, scroll->h);

    d->background_render = 1;
    scroll->render = 1;

    if (scroll->ctx && scroll->ctx->window)
        scroll->ctx->window->invalid = 1;
}

void scroll_set_offset(component_t *scroll, int offset)
{
    if (!scroll || scroll->type != UI_SCROLL)
        return;

    scroll_data_t *d = scroll->data;

    if (!d)
        return;

    d->scroll_offset = offset;

    scroll_clamp_offset(d, scroll->h);

    d->background_render = 1;
    scroll->render = 1;

    if (scroll->ctx && scroll->ctx->window)
        scroll->ctx->window->invalid = 1;
}


static void scroll_get_metrics(
    scroll_data_t *d,
    int height,
    int *thumb_h,
    int *max_offset,
    int *max_thumb_y
)
{
    if (!thumb_h || !max_offset || !max_thumb_y)
        return;

    *thumb_h = 0;
    *max_offset = 0;
    *max_thumb_y = 0;

    if (!d || height <= 0)
        return;

    int content_height = d->content_height;

    if (content_height <= height)
        return;

    int64_t numerator = (int64_t)height * height;
    int th = (int)(numerator / content_height);

    if (th < 20 && height >= 20)
        th = 20;

    if (th > height)
        th = height;

    *thumb_h = th;
    *max_offset = content_height - height;
    *max_thumb_y = height - th;
}

int draw_scroll(component_t *c, window_t *win)
{
    if (!c || !win || !c->visible || !c->render)
        return 0;

    scroll_data_t *d = c->data;

    if (!d)
        return 0;

    ui_lock(&win->lock);

    scroll_clamp_offset(d, c->h);

    int scrollbar_w = d->scrollbar_width;
    int viewport_w = c->w - scrollbar_w;

    if (viewport_w < 0)
        viewport_w = 0;

    int redraw_children = d->background_render;
    if (d->background_render) {
        fill_rect(
            d->ctx->window->buffer,
            d->ctx->window->width,
            d->ctx->window->height,
            d->ctx->window->x,
            d->ctx->window->y,
            d->ctx->window->width,
            d->ctx->window->height,
            d->ctx->window->bg
        );

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
            d->border_thickness,
            d->border_radius);

        d->background_render = 0;
    }

    int render = 0;
    for (int i = 0; i < d->child_count; i++)
    {
        component_t *child = d->children[i];

        if (!child || !child->visible || !child->on_draw)
            continue;

        if (redraw_children)
            child->render = 1;

        render += child->on_draw(child, d->ctx->window);
    }

    // CÓPIA DA JANELA
    if (render > 0 || redraw_children)
    window_copy_buffer(
        win,
        d->ctx->window,
        d->scroll_offset,
        d->width,
        d->height,
        c->x + d->border_thickness,
        c->y + d->border_thickness);

    int track_x = c->x + c->w - scrollbar_w;

    fill_rect(
        win->buffer,
        win->width,
        win->height,
        track_x,
        c->y,
        scrollbar_w,
        c->h,
        argb(255, 230, 230, 230)
    );

    if (d->content_height > c->h)
    {
        int thumb_h;
        int max_offset;
        int max_thumb_y;

        scroll_get_metrics(
            d,
            c->h,
            &thumb_h,
            &max_offset,
            &max_thumb_y);

        int thumb_y = 0;

        if (max_offset > 0)
        {
            thumb_y = (int)(((int64_t)d->scroll_offset * max_thumb_y) /
                            max_offset);
        }

        fill_rect(
            win->buffer,
            win->width,
            win->height,
            track_x,
            c->y + thumb_y,
            scrollbar_w,
            thumb_h,
            argb(255, 160, 160, 160));
    }

    c->render = 0;

    ui_unlock(&win->lock);

    return 1;
}



int scroll_event(component_t *c, event_t ev)
{
    if (!c || !c->visible || c->type != UI_SCROLL)
        return 0;

    scroll_data_t *d = c->data;

    if (!d)
        return 0;

    int scrollbar_x = c->x + c->w - d->scrollbar_width;

    if (ev.type == EVENT_MOUSE_LEFT_DOWN) {
        if (!scroll_contains(c, ev.mouse.x, ev.mouse.y))
            return 0;

        if (d->content_height > c->h &&
            ev.mouse.x >= scrollbar_x) {

            int thumb_h = 0;
            int max_offset = 0;
            int max_thumb_y = 0;

            scroll_get_metrics(
                d,
                c->h,
                &thumb_h,
                &max_offset,
                &max_thumb_y
            );

            int thumb_y = 0;

            if (max_offset > 0 && max_thumb_y > 0) {
                thumb_y = (int)(
                    ((int64_t)d->scroll_offset * max_thumb_y) /
                    max_offset
                );
            }

            int mouse_y = ev.mouse.y - c->y;

            if (mouse_y >= thumb_y &&
                mouse_y < thumb_y + thumb_h) {

                d->dragging_scroll = 1;
                d->drag_offset_y = mouse_y - thumb_y;

            } else {
                int new_thumb_y = mouse_y - thumb_h / 2;

                if (new_thumb_y < 0)
                    new_thumb_y = 0;

                if (new_thumb_y > max_thumb_y)
                    new_thumb_y = max_thumb_y;

                if (max_thumb_y > 0) {
                    d->scroll_offset = (int)(
                        ((int64_t)new_thumb_y * max_offset) /
                        max_thumb_y
                    );
                }
            }

            scroll_clamp_offset(d, c->h);

            d->background_render = 1;
            c->render = 1;

            if (c->ctx && c->ctx->window)
                c->ctx->window->invalid = 1;

            return c->render;
        }
    }

    if (ev.type == EVENT_MOUSE_MOVE && d->dragging_scroll) {

        int thumb_h = 0;
        int max_offset = 0;
        int max_thumb_y = 0;

        scroll_get_metrics(
            d,
            c->h,
            &thumb_h,
            &max_offset,
            &max_thumb_y
        );

        if (max_offset > 0 && max_thumb_y > 0) {

            int new_thumb_y =
                ev.mouse.y - c->y - d->drag_offset_y;

            if (new_thumb_y < 0)
                new_thumb_y = 0;

            if (new_thumb_y > max_thumb_y)
                new_thumb_y = max_thumb_y;

            d->scroll_offset = (int)(
                ((int64_t)new_thumb_y * max_offset) /
                max_thumb_y
            );

            scroll_clamp_offset(d, c->h);

            d->background_render = 1;
            c->render = 1;

            if (c->ctx && c->ctx->window)
                c->ctx->window->invalid = 1;
        }

        return c->render;
    }

    if (ev.type == EVENT_MOUSE_UP && d->dragging_scroll) {
        d->dragging_scroll = 0;
    }

    if ((ev.type == EVENT_MOUSE_LEFT_DOWN || ev.type == EVENT_MOUSE_MOVE ) &&
        !scroll_contains(c, ev.mouse.x, ev.mouse.y))
        return c->render;

    int event = 0;
    int viewport_w = c->w - d->scrollbar_width;

    if (viewport_w < 0)
        viewport_w = 0;

    for (int i = d->child_count - 1; i >= 0; i--)
    {

        component_t *child = d->children[i];

        if (!child || !child->visible || !child->on_event)
            continue;

        event_t child_ev = ev;

        child_ev.mouse.x = ev.mouse.x - c->x;
        child_ev.mouse.y = ev.mouse.y - c->y + d->scroll_offset;

        event += child->on_event(child, child_ev);
    }

    if(event > 0) c->render = 1;

    return c->render;
}