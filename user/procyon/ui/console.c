#include "include/components.h"
#include "include/console.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

static int console_clamp_offset(console_data_t *d, int height)
{
    if (!d)
        return 0;

    int max_offset = d->content_height - height;

    if (max_offset < 0)
        max_offset = 0;

    if (d->scroll_offset < 0)
        d->scroll_offset = 0;

    if (d->scroll_offset > max_offset)
        d->scroll_offset = max_offset;

    return max_offset;
}

static int console_contains(component_t *c, int x, int y)
{
    return x >= c->x &&
           y >= c->y &&
           x < c->x + c->w &&
           y < c->y + c->h;
}

static void console_invalidate(component_t *c)
{
    if (!c)
        return;

    c->render = 1;

    if (c->ctx && c->ctx->window)
        c->ctx->window->invalid = 1;
}

static int console_resize_buffer(console_data_t *d, int required_height)
{
    if (!d || !d->ctx->window || required_height <= 0)
        return -1;

    window_t *win = d->ctx->window;

    if (required_height <= win->height)
        return 0;

    if (win->width <= 0 || win->height <= 0 || !win->buffer)
        return -1;
    
    ui_lock(&win->lock);

    size_t width = (size_t)win->width;
    size_t height = (size_t)required_height;

    size_t new_pixels = width * height;

    size_t old_pixels = width * (size_t)win->height;
    
    uint32_t *new_buffer = realloc(
        win->buffer,
        new_pixels * sizeof(uint32_t)
    );
    
    if (!new_buffer) {
        ui_unlock(&win->lock);
        return -1;
    }

    for (size_t i = old_pixels; i < new_pixels; i++)
        new_buffer[i] = win->bg;

    win->buffer = new_buffer;
    win->height = required_height;

    ui_unlock(&win->lock);

    return 0;
}

component_t *create_console(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h
)
{
    if (!ctx || !ctx->window ||
        w <= CONSOLE_SCROLLBAR_WIDTH || h <= 0)
        return NULL;

    component_t *c = calloc(1, sizeof(component_t));

    if (!c)
        return NULL;

    console_data_t *d = calloc(1, sizeof(console_data_t));

    if (!d) {
        free(c);
        return NULL;
    }

    d->background_render = 1;
    d->border_radius = 0;
    d->border_thickness = 0;

    d->ctx = malloc(sizeof(ui_context_t));
    if (!d->ctx) {
        free(d);
        free(c);
        return NULL;
    }

    memset(d->ctx, 0, sizeof(ui_context_t));

    d->scrollbar_width = CONSOLE_SCROLLBAR_WIDTH;
    d->background_render = 1;

    int viewport_width = w - d->scrollbar_width;

    d->ctx->fd = ctx->fd;
    d->width = w - (d->scrollbar_width+(d->border_thickness*2));
    d->height = h - (d->border_thickness*2);

    d->ctx->window = component_window_create(
        ctx->window,
        viewport_width,
        h,
        argb(ctx->window->bg >> 24 &0xFF, 16, 18, 22),
        0
    );

    if (!d->ctx->window) {
        free(d);
        free(c);
        return NULL;
    }

    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;
    c->type = UI_CONSOLE;
    c->visible = 1;
    c->render = 1;
    c->ctx = ctx;
    c->data = d;

    c->bg = argb(255, 16, 18, 22);
    c->border_color = c->bg;

    unsigned char *free_font_buffer = 0;
    void *font = malloc(sizeof(stbtt_fontinfo));
    if (font) {
        memset(font, 0, sizeof(stbtt_fontinfo));
        if (stbtt_font_init(font, "usr/share/fonts/UbuntuMono.ttf", &free_font_buffer) == 0)
        {
            c->free_font_buffer = free_font_buffer;
            c->font = *(stbtt_fontinfo *)font;
        }
        else
        {
            free(font);
            c->font = *(stbtt_fontinfo *)ctx->window->font;
        }
    }else {
         c->font = *(stbtt_fontinfo *)ctx->window->font;   
    }
    c->font_size = 18;
    c->fg = argb(255, 230, 230, 230);

    d->char_width = c->font_size/2;
    d->char_height = c->font_size;

    c->on_draw = draw_console;
    c->on_event = console_event;

    // --- inicializar cache da fonte aqui ---
    build_font_cache(&c->font, c->font_size, &c->font_cache);

    if (ui_add_component(ctx, c) != 0) {
        free(d->ctx->window->buffer);
        free(d->ctx->window);
        free(d);
        free(c);
        return NULL;
    }

    return c;
}

void console_set_content_height(component_t *console, int content_height)
{
    if (!console || console->type != UI_CONSOLE)
        return;

    console_data_t *d = console->data;

    if (!d || content_height < 0)
        return;

    if (content_height < console->h)
        content_height = console->h;

    if (console_resize_buffer(d, content_height) != 0)
        return;

    d->content_height = content_height;

    console_clamp_offset(d, console->h);

    d->background_render = 1;

    console_invalidate(console);
}

void console_set_offset(component_t *console, int offset)
{
    if (!console || console->type != UI_CONSOLE)
        return;

    console_data_t *d = console->data;

    if (!d)
        return;

    d->scroll_offset = offset;

    console_clamp_offset(d, console->h);

    d->background_render = 1;

    console_invalidate(console);
}

static void console_get_metrics(
    console_data_t *d,
    int height,
    int *thumb_height,
    int *max_offset,
    int *max_thumb_y
)
{
    if (!thumb_height || !max_offset || !max_thumb_y)
        return;

    *thumb_height = 0;
    *max_offset = 0;
    *max_thumb_y = 0;

    if (!d || height <= 0)
        return;

    if (d->content_height <= height)
        return;

    int64_t numerator = (int64_t)height * height;

    int thumb = (int)(numerator / d->content_height);

    if (thumb < 20 && height >= 20)
        thumb = 20;

    if (thumb > height)
        thumb = height;

    *thumb_height = thumb;
    *max_offset = d->content_height - height;
    *max_thumb_y = height - thumb;
}

static int console_is_at_bottom(component_t *console)
{
    if (!console)
        return 1;

    console_data_t *d = console->data;

    if (!d)
        return 1;

    int max_offset = d->content_height - console->h;

    return max_offset <= 0 || d->scroll_offset >= max_offset;
}

static int console_ensure_height(component_t *console, int required_height)
{
    if (!console)
        return -1;

    console_data_t *d = console->data;

    if (!d || !d->ctx || !d->ctx->window)
        return -1;

    if (required_height < console->h)
        required_height = console->h;

    if (required_height <= d->content_height)
        return 0;

    int was_at_bottom = console_is_at_bottom(console);

    console_set_content_height(console, required_height);

    if (d->content_height < required_height)
        return -1;

    if (was_at_bottom)
        console_set_offset(console, INT_MAX);

    return 0;
}

int console_newline(component_t *console)
{
    if (!console || console->type != UI_CONSOLE)
        return -1;

    console_data_t *d = console->data;

    if (!d || d->char_height <= 0)
        return -1;

    d->cursor_x = 0;

    if (d->cursor_y > INT_MAX - d->char_height)
        return -1;

    d->cursor_y += d->char_height;

    if (console_ensure_height(
            console,
            d->cursor_y + d->char_height) != 0)
        return -1;

    console->render = 1;

    return 0;
}

int console_put_char(component_t *console, char ch)
{
    if (!console || console->type != UI_CONSOLE)
        return -1;

    console_data_t *d = console->data;

    if (!d || !d->ctx || !d->ctx->window)
        return -1;

    window_t *win = d->ctx->window;

    if (d->char_width <= 0 || d->char_height <= 0)
        return -1;

    if (ch == '\n')
        return console_newline(console);

    if (ch == '\r') {
        d->cursor_x = 0;
        console->render = 1;
        return 0;
    }

    if (ch == '\b') {
        if (d->cursor_x >= d->char_width) {
            d->cursor_x -= d->char_width;

            fill_rect(
                win->buffer,
                win->width,
                win->height,
                d->cursor_x,
                d->cursor_y,
                d->char_width,
                d->char_height,
                win->bg
            );

            console->render = 1;
        }

        return 0;
    }

    if (ch == '\t') {
        int spaces = 4 - ((d->cursor_x / d->char_width) % 4);

        while (spaces-- > 0) {
            if (console_put_char(console, ' ') != 0)
                return -1;
        }

        return 0;
    }

    if (ch < 32)
        return 0;

    if (d->cursor_x + d->char_width > d->width) {
        if (console_newline(console) != 0)
            return -1;
    }

    if (console_ensure_height(
            console,
            d->cursor_y + d->char_height) != 0)
        return -1;

    char text[2] = {ch, '\0'};

    draw_text_cached(
        &console->font_cache,
        win->buffer,
        win->width,
        win->height,
        text,
        d->cursor_x,
        d->cursor_y,
        d->width,
        console->fg
    );

    d->cursor_x += d->char_width;

    console->render = 1;

    return 0;
}

int console_write(component_t *console, const char *text)
{
    if (!console || !text)
        return -1;

    while (*text) {
        if (console_put_char(console, *text++) != 0)
            return -1;
    }

    return 0;
}

int draw_console(component_t *c, window_t *win)
{
    if (!c || !win || !c->visible || !c->render)
        return 0;

    console_data_t *d = c->data;

    if (!d || !d->ctx->window)
        return 0;

    ui_lock(&win->lock);

    console_clamp_offset(d, c->h);

    if (d->background_render) {
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
            0,
            0
        );

        d->background_render = 0;
    }

    window_copy_buffer(
        win,
        d->ctx->window,
        d->scroll_offset,
        d->width,
        d->height,
        c->x + d->border_thickness,
        c->y + d->border_thickness);

    int track_x = c->x + c->w - d->scrollbar_width;

    fill_rect(
        win->buffer,
        win->width,
        win->height,
        track_x,
        c->y,
        d->scrollbar_width,
        c->h,
        argb(255, 35, 37, 42)
    );

    if (d->content_height > c->h) {
        int thumb_height;
        int max_offset;
        int max_thumb_y;

        console_get_metrics(
            d,
            c->h,
            &thumb_height,
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

        fill_rect(
            win->buffer,
            win->width,
            win->height,
            track_x,
            c->y + thumb_y,
            d->scrollbar_width,
            thumb_height,
            argb(255, 120, 125, 135)
        );
    }

    c->render = 0;

    ui_unlock(&win->lock);

    return 1;
}

int console_event(component_t *c, event_t ev)
{
    if (!c || !c->visible || c->type != UI_CONSOLE)
        return 0;

    console_data_t *d = c->data;

    if (!d)
        return 0;

    int scrollbar_x = c->x + c->w - d->scrollbar_width;

    if (ev.type == EVENT_MOUSE_LEFT_DOWN) {
        if (!console_contains(c, ev.mouse.x, ev.mouse.y))
            return 0;

        if (d->content_height > c->h &&
            ev.mouse.x >= scrollbar_x) {

            int thumb_height;
            int max_offset;
            int max_thumb_y;

            console_get_metrics(
                d,
                c->h,
                &thumb_height,
                &max_offset,
                &max_thumb_y
            );

            if (max_thumb_y <= 0 || max_offset <= 0)
                return 0;

            int thumb_y = (int)(
                ((int64_t)d->scroll_offset * max_thumb_y) /
                max_offset
            );

            int mouse_y = ev.mouse.y - c->y;

            if (mouse_y >= thumb_y &&
                mouse_y < thumb_y + thumb_height) {

                d->dragging_scroll = 1;
                d->drag_offset_y = mouse_y - thumb_y;

            } else {
                int new_thumb_y = mouse_y - thumb_height / 2;

                if (new_thumb_y < 0)
                    new_thumb_y = 0;

                if (new_thumb_y > max_thumb_y)
                    new_thumb_y = max_thumb_y;

                d->scroll_offset = (int)(
                    ((int64_t)new_thumb_y * max_offset) /
                    max_thumb_y
                );
            }

            d->background_render = 1;
            console_invalidate(c);

            return 1;
        }
    }

    if (ev.type == EVENT_MOUSE_MOVE && d->dragging_scroll) {
        int thumb_height;
        int max_offset;
        int max_thumb_y;

        console_get_metrics(
            d,
            c->h,
            &thumb_height,
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

            console_clamp_offset(d, c->h);

            d->background_render = 1;
            console_invalidate(c);
        }

        return c->render;
    }

    if (ev.type == EVENT_MOUSE_UP && d->dragging_scroll) {
        d->dragging_scroll = 0;
        return 1;
    }

    return 0;
}