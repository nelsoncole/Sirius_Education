#include "include/components.h"
#include "include/textbox.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define TEXTBOX_SCROLLBAR_WIDTH 14

static int textbox_view_width(component_t *c, textbox_data_t *t)
{
    int width = c->w - t->border_thickness * 2;
    if (t->show_vertical_scrollbar)
        width -= t->scrollbar_width;
    return width > 1 ? width : 1;
}

static int textbox_view_height(component_t *c, textbox_data_t *t)
{
    int height = c->h - t->border_thickness * 2;
    if (t->show_horizontal_scrollbar)
        height -= t->scrollbar_width;
    return height > 1 ? height : 1;
}

static void textbox_invalidate(component_t *c)
{
    if (!c)
        return;

    c->render = 1;
    c->dirty = 1;
}

static int textbox_contains(component_t *c, int x, int y)
{
    if (!c || c->w <= 0 || c->h <= 0)
        return 0;

    return (int64_t)x >= c->x && (int64_t)y >= c->y &&
           (int64_t)x < (int64_t)c->x + c->w &&
           (int64_t)y < (int64_t)c->y + c->h;
}

static int textbox_rebuild(component_t *c, textbox_data_t *t);
static void textbox_on_clear(component_t *c)
{
    if (!c || !c->data)
        return;

    textbox_data_t *t = (textbox_data_t *)c->data;

    ui_lock(&t->spinlock);
    if (t->text)
        t->text[0] = '\0';
    t->text_length = 0;
    t->scroll_offset_x = 0;
    t->scroll_offset_y = 0;
    t->dragging_vertical = 0;
    t->dragging_horizontal = 0;
    textbox_rebuild(c, t);
    textbox_invalidate(c);
    ui_unlock(&t->spinlock);
}

static void textbox_on_enable(component_t *c)
{
    textbox_invalidate(c);
}

static void textbox_on_disable(component_t *c)
{
    if (c && c->data) {
        textbox_data_t *t = (textbox_data_t *)c->data;
        ui_lock(&t->spinlock);
        t->dragging_vertical = 0;
        t->dragging_horizontal = 0;
        ui_unlock(&t->spinlock);
    }

    textbox_invalidate(c);
}

static void textbox_on_destroy(component_t *c)
{
    if (!c)
        return;

    textbox_data_t *t = (textbox_data_t *)c->data;
    if (t) {
        if (t->ctx) {
            if (t->ctx->window) {
                free(t->ctx->window->buffer);
                t->ctx->window->buffer = NULL;
                free(t->ctx->window);
                t->ctx->window = NULL;
            }

            free(t->ctx);
            t->ctx = NULL;
        }

        free(t->text);
        free(t->lines);
        free(t);
        c->data = NULL;
    }

    free(c->free_font_buffer);
    c->free_font_buffer = NULL;
}

static int textbox_resize_buffer(textbox_data_t *t, int width, int height)
{
    if (!t || !t->ctx || !t->ctx->window)
        return -1;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    window_t *win = t->ctx->window;
    if (win->width == width && win->height == height && win->buffer)
        return 0;
    if ((size_t)width > SIZE_MAX / (size_t)height)
        return -1;
    size_t pixels = (size_t)width * (size_t)height;
    if (pixels > SIZE_MAX / sizeof(uint32_t))
        return -1;

    ui_lock(&win->lock);
    uint32_t *buffer = realloc(win->buffer, pixels * sizeof(uint32_t));
    if (!buffer) {
        ui_unlock(&win->lock);
        return -1;
    }
    win->buffer = buffer;
    win->width = width;
    win->height = height;
    for (size_t i = 0; i < pixels; ++i)
        buffer[i] = win->bg;
    win->invalid = 1;
    ui_unlock(&win->lock);
    return 0;
}

static void textbox_copy_view(
    window_t *dst, window_t *src,
    int source_x, int source_y,
    int width, int height,
    int dest_x, int dest_y
)
{
    if (!dst || !src || !dst->buffer || !src->buffer)
        return;

    for (int y = 0; y < height; ++y) {
        int sy = source_y + y;
        int dy = dest_y + y;
        if (sy < 0 || sy >= src->height || dy < 0 || dy >= dst->height)
            continue;
        for (int x = 0; x < width; ++x) {
            int sx = source_x + x;
            int dx = dest_x + x;
            if (sx < 0 || sx >= src->width || dx < 0 || dx >= dst->width)
                continue;
            dst->buffer[(size_t)dy * dst->width + dx] =
                src->buffer[(size_t)sy * src->width + sx];
        }
    }
}

static void textbox_get_thumb(
    int viewport, int content,
    int *thumb_size, int *max_thumb_offset,
    int *max_content_offset, int min_thumb
)
{
    *thumb_size = viewport;
    *max_thumb_offset = 0;
    *max_content_offset = 0;
    if (viewport <= 0 || content <= viewport)
        return;

    int thumb = (int)(((int64_t)viewport * viewport) / content);
    if (min_thumb < 1) min_thumb = 1;
    if (thumb < min_thumb && viewport >= min_thumb)
        thumb = min_thumb;
    if (thumb > viewport) thumb = viewport;

    *thumb_size = thumb;
    *max_thumb_offset = viewport - thumb;
    *max_content_offset = content - viewport;
}

typedef struct {
    int monospace;
    int char_width;
} textbox_font_metrics_t;

static int textbox_measure_raw(component_t *c, const char *text, size_t length)
{
    if (!c || (!text && length) || length > INT_MAX || !length)
        return 0;

    char *temporary = malloc(length + 1);
    if (!temporary)
        return 0;

    memcpy(temporary, text, length);
    temporary[length] = '\0';

    int width = text_get_string_width(
        &c->font, (float)c->font_size, temporary, (int)length
    );

    free(temporary);
    return width > 0 ? width : 0;
}

static textbox_font_metrics_t textbox_get_font_metrics(component_t *c)
{
    static const char *samples[] = {"i", "W", "m", "0", " "};
    textbox_font_metrics_t metrics = {0, 0};

    if (!c)
        return metrics;

    int reference = textbox_measure_raw(c, "M", 1);
    if (reference < 1)
        reference = 1;

    metrics.char_width = reference;
    metrics.monospace = 1;

    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        if (textbox_measure_raw(c, samples[i], 1) != reference) {
            metrics.monospace = 0;
            break;
        }
    }

    return metrics;
}

static int textbox_measure(
    component_t *c,
    const char *text,
    size_t length,
    const textbox_font_metrics_t *metrics
)
{
    if (!c || (!text && length) || length > INT_MAX || !length)
        return 0;

    if (metrics && metrics->monospace) {
        if (length > (size_t)INT_MAX / (size_t)metrics->char_width)
            return INT_MAX;
        return (int)length * metrics->char_width;
    }

    return textbox_measure_raw(c, text, length);
}

static int textbox_reserve_lines(textbox_data_t *t, size_t required)
{
    if (required <= t->line_capacity)
        return 0;
    size_t capacity = t->line_capacity ? t->line_capacity * 2 : 16;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2)
            return -1;
        capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(textbox_line_t))
        return -1;
    textbox_line_t *lines = realloc(t->lines, capacity * sizeof(*lines));
    if (!lines)
        return -1;
    t->lines = lines;
    t->line_capacity = capacity;
    return 0;
}

static int textbox_add_line(textbox_data_t *t, size_t start, size_t length, int width)
{
    if (textbox_reserve_lines(t, t->line_count + 1) != 0)
        return -1;
    t->lines[t->line_count].start = start;
    t->lines[t->line_count].length = length;
    t->lines[t->line_count].width = width;
    t->line_count++;
    return 0;
}

static int textbox_build_layout(component_t *c, textbox_data_t *t, int wrap_width)
{
    textbox_font_metrics_t metrics = textbox_get_font_metrics(c);
    t->line_count = 0;
    int max_width = 0;
    size_t paragraph_start = 0;

    if (wrap_width < 1)
        wrap_width = 1;

    while (paragraph_start <= t->text_length) {
        size_t paragraph_end = paragraph_start;
        while (paragraph_end < t->text_length && t->text[paragraph_end] != '\n')
            paragraph_end++;

        if (paragraph_start == paragraph_end) {
            if (textbox_add_line(t, paragraph_start, 0, 0) != 0)
                return -1;
        } else if (!t->word_wrap) {
            int width = textbox_measure(
                c, t->text + paragraph_start,
                paragraph_end - paragraph_start, &metrics
            );
            if (textbox_add_line(t, paragraph_start,
                                 paragraph_end - paragraph_start, width) != 0)
                return -1;
            if (width > max_width)
                max_width = width;
        } else {
            size_t start = paragraph_start;
            while (start < paragraph_end) {
                size_t end = start;
                size_t last_space = SIZE_MAX;
                int width = 0;
                int last_space_width = 0;

                while (end < paragraph_end) {
                    int next_width = textbox_measure(
                        c, t->text + start, end - start + 1, &metrics
                    );
                    if (next_width > wrap_width && end > start)
                        break;

                    width = next_width;
                    if (t->text[end] == ' ') {
                        last_space = end;
                        last_space_width = width;
                    }
                    end++;
                    if (width > wrap_width)
                        break;
                }

                if (end < paragraph_end && last_space != SIZE_MAX && last_space >= start) {
                    end = last_space;
                    width = last_space_width;
                    while (end > start && t->text[end - 1] == ' ')
                        end--;
                    width = textbox_measure(c, t->text + start, end - start, &metrics);
                }

                if (end == start) {
                    end = start + 1;
                    width = textbox_measure(c, t->text + start, 1, &metrics);
                }

                if (textbox_add_line(t, start, end - start, width) != 0)
                    return -1;
                if (width > max_width)
                    max_width = width;

                start = end;
                while (start < paragraph_end && t->text[start] == ' ')
                    start++;
            }
        }

        if (paragraph_end == t->text_length)
            break;
        paragraph_start = paragraph_end + 1;
        if (paragraph_start == t->text_length) {
            if (textbox_add_line(t, paragraph_start, 0, 0) != 0)
                return -1;
            break;
        }
    }

    if (t->line_count == 0 && textbox_add_line(t, 0, 0, 0) != 0)
        return -1;

    if (t->line_count > (size_t)(INT_MAX / t->char_height))
        return -1;

    t->content_width = max_width;
    t->content_height = (int)t->line_count * t->char_height;
    return 0;
}

static int textbox_render_viewport(
    component_t *c,
    textbox_data_t *t,
    window_t *win,
    int view_width,
    int view_height
)
{
    if (!c || !t || !win || !win->buffer)
        return -1;

    for (int y = 0; y < win->height; ++y) {
        for (int x = 0; x < win->width; ++x)
            win->buffer[(size_t)y * win->width + x] = win->bg;
    }

    textbox_font_metrics_t metrics = textbox_get_font_metrics(c);
    int base_y = 0;
    if (t->content_height <= view_height) {
        int text_height = (int)t->line_count * t->char_height;
        if (t->vertical_align == EDITBOX_ALIGN_VCENTER)
            base_y = (view_height - text_height) / 2;
        else if (t->vertical_align == EDITBOX_ALIGN_BOTTOM)
            base_y = view_height - text_height;
    }

    int y = base_y - t->scroll_offset_y;
    size_t first_line = (size_t)(t->scroll_offset_y / t->char_height);
    if (first_line > 0)
        first_line--;

    for (size_t i = first_line; i < t->line_count; ++i) {
        int line_y = y + (int)i * t->char_height;
        if (line_y >= view_height)
            break;
        if (line_y + t->char_height <= 0)
            continue;

        textbox_line_t *line = &t->lines[i];
        int x = 0;
        if (t->content_width <= view_width && line->width < view_width) {
            if (t->horizontal_align == EDITBOX_ALIGN_CENTER)
                x = (view_width - line->width) / 2;
            else if (t->horizontal_align == EDITBOX_ALIGN_RIGHT)
                x = view_width - line->width;
        }
        x -= t->scroll_offset_x;

        if (!line->length)
            continue;

        char *line_text = malloc(line->length + 1);
        if (!line_text)
            return -1;
        memcpy(line_text, t->text + line->start, line->length);
        line_text[line->length] = '\0';

        const char *visible_text = line_text;
        int draw_x = x;
        while (*visible_text && draw_x < 0) {
            int char_width = metrics.monospace
                ? metrics.char_width
                : textbox_measure_raw(c, visible_text, 1);
            if (char_width < 1)
                char_width = 1;
            draw_x += char_width;
            visible_text++;
        }

        if (*visible_text && draw_x < view_width) {
            draw_text_cached(
                &c->font_cache,
                win->buffer,
                win->width,
                win->height,
                visible_text,
                draw_x,
                line_y,
                view_width - draw_x,
                c->fg
            );
        }

        free(line_text);
    }

    return 0;
}

static int textbox_rebuild(component_t *c, textbox_data_t *t)
{
    if (!c || !t || !t->ctx || !t->ctx->window ||
        !t->ctx->window->buffer || t->char_height <= 0)
        return -1;

    int view_width = textbox_view_width(c, t);
    int view_height = textbox_view_height(c, t);

    if (textbox_build_layout(c, t, view_width) != 0)
        return -1;

    if (t->content_width < view_width)
        t->content_width = view_width;
    if (t->content_height < view_height)
        t->content_height = view_height;

    int max_x = t->content_width - view_width;
    int max_y = t->content_height - view_height;
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    if (t->scroll_offset_x < 0) t->scroll_offset_x = 0;
    if (t->scroll_offset_y < 0) t->scroll_offset_y = 0;
    if (t->scroll_offset_x > max_x) t->scroll_offset_x = max_x;
    if (t->scroll_offset_y > max_y) t->scroll_offset_y = max_y;

    if (textbox_resize_buffer(t, view_width, view_height) != 0)
        return -1;

    window_t *win = t->ctx->window;
    ui_lock(&win->lock);
    int result = textbox_render_viewport(c, t, win, view_width, view_height);
    ui_unlock(&win->lock);
    return result;
}

component_t *create_textbox(
    ui_context_t *ctx, int x, int y, int w, int h, int font_size)
{
    if (!ctx || !ctx->window || !ctx->window->font ||
        w <= 0 || h <= 0 || font_size <= 0)
        return NULL;

    component_t *c = calloc(1, sizeof(*c));
    textbox_data_t *t = calloc(1, sizeof(*t));
    if (!c || !t) {
        free(c);
        free(t);
        return NULL;
    }

    t->ctx = calloc(1, sizeof(*t->ctx));
    if (!t->ctx) {
        free(c);
        free(t);
        return NULL;
    }
    t->ctx->fd = ctx->fd;
    t->ctx->suppress_server_notify = 1;
    t->font_size = font_size;
    t->char_height = font_size;
    t->horizontal_align = EDITBOX_ALIGN_LEFT;
    t->vertical_align = EDITBOX_ALIGN_TOP;
    t->border_radius = 5;
    t->border_thickness = 1;
    t->scrollbar_width = TEXTBOX_SCROLLBAR_WIDTH;
    t->show_vertical_scrollbar = 1;
    t->show_horizontal_scrollbar = 0;
    t->word_wrap = 1;
    t->scrollbar_bg = argb(255, 38, 40, 46);
    t->scrollbar_fg = argb(255, 120, 125, 135);
    t->text = calloc(1, 1);
    if (!t->text) {
        free(t->ctx);
        free(t);
        free(c);
        return NULL;
    }

    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;
    c->type = UI_TEXTBOX;
    c->visible = 1;
    c->enabled = 1;
    c->render = 1;
    c->dirty = 1;
    c->ctx = ctx;
    c->data = t;
    c->bg = argb(255, 30, 32, 38);
    c->fg = argb(255, 235, 235, 235);
    c->border_color = argb(255, 80, 85, 95);
    c->font_size = font_size;

    unsigned char *font_buffer = NULL;
    stbtt_fontinfo *font = malloc(sizeof(*font));
    if (font) {
        memset(font, 0, sizeof(*font));
        if (stbtt_font_init(font, "usr/share/fonts/Ubuntu.ttf", &font_buffer) == 0) {
            c->free_font_buffer = font_buffer;
            c->font = *font;
        } else {
            c->font = *(stbtt_fontinfo *)ctx->window->font;
        }
        free(font);
    } else {
        c->font = *(stbtt_fontinfo *)ctx->window->font;
    }

    t->ctx->window = component_window_create(
        ctx->window,
        textbox_view_width(c, t),
        textbox_view_height(c, t),
        c->bg,
        0
    );
    if (!t->ctx->window) {
        free(t->text);
        free(t->ctx);
        free(t);
        free(c->free_font_buffer);
        free(c);
        return NULL;
    }

    build_font_cache(&c->font, c->font_size, &c->font_cache);
    c->on_draw = draw_textbox;
    c->on_event = textbox_event;
    c->on_clear = textbox_on_clear;
    c->on_enable = textbox_on_enable;
    c->on_disable = textbox_on_disable;
    c->on_destroy = textbox_on_destroy;

    if (ui_add_component(ctx, c) != 0) {
        if(c->on_destroy) 
            c->on_destroy(c);

        free(c);
        return NULL;
    }
    if (textbox_rebuild(c, t) != 0) {
        /*
         * O componente já foi registado no contexto. Mantemos a instância
         * válida e marcada para renderização; a próxima actualização pode
         * tentar reconstruir o conteúdo novamente.
         */
        textbox_invalidate(c);
    }

    textbox_invalidate(c);
    return c;
}

void textbox_set_text(component_t *c, const char *text)
{
    if (!c || c->type != UI_TEXTBOX)
        return;

    if (!text)
        text = "";
    textbox_data_t *t = c->data;
    if (!t)
        return;

    size_t length = strlen(text);
    if (length > INT_MAX)
        return;
    char *copy = malloc(length + 1);
    if (!copy)
        return;
    memcpy(copy, text, length + 1);

    ui_lock(&t->spinlock);
    free(t->text);
    t->text = copy;
    t->text_length = length;
    t->scroll_offset_x = 0;
    t->scroll_offset_y = 0;
    textbox_rebuild(c, t);
    textbox_invalidate(c);
    ui_unlock(&t->spinlock);
}

void textbox_set_alignment(
    component_t *c,
    editbox_horizontal_align_t horizontal,
    editbox_vertical_align_t vertical)
{
    if (!c || c->type != UI_TEXTBOX ||
        horizontal < EDITBOX_ALIGN_LEFT || horizontal > EDITBOX_ALIGN_RIGHT ||
        vertical < EDITBOX_ALIGN_TOP || vertical > EDITBOX_ALIGN_BOTTOM)
        return;
    textbox_data_t *t = c->data;
    if (!t) return;
    ui_lock(&t->spinlock);
    t->horizontal_align = horizontal;
    t->vertical_align = vertical;
    textbox_rebuild(c, t);
    textbox_invalidate(c);
    ui_unlock(&t->spinlock);
}

void textbox_set_scrollbars(component_t *c, int vertical, int horizontal)
{
    if (!c || c->type != UI_TEXTBOX) return;
    textbox_data_t *t = c->data;
    if (!t) return;
    ui_lock(&t->spinlock);
    t->show_vertical_scrollbar = !!vertical;
    t->show_horizontal_scrollbar = !!horizontal;
    if (!horizontal) t->scroll_offset_x = 0;
    if (!vertical) t->scroll_offset_y = 0;
    textbox_rebuild(c, t);
    textbox_invalidate(c);
    ui_unlock(&t->spinlock);
}

void textbox_set_word_wrap(component_t *c, int enabled)
{
    if (!c || c->type != UI_TEXTBOX) return;
    textbox_data_t *t = c->data;
    if (!t) return;
    ui_lock(&t->spinlock);
    t->word_wrap = !!enabled;
    if (t->word_wrap) t->scroll_offset_x = 0;
    textbox_rebuild(c, t);
    textbox_invalidate(c);
    ui_unlock(&t->spinlock);
}

void textbox_set_offset(component_t *c, int offset_x, int offset_y)
{
    if (!c || c->type != UI_TEXTBOX) return;
    textbox_data_t *t = c->data;
    if (!t) return;
    ui_lock(&t->spinlock);
    int max_x = t->content_width - textbox_view_width(c, t);
    int max_y = t->content_height - textbox_view_height(c, t);
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    if (offset_x < 0) offset_x = 0;
    if (offset_y < 0) offset_y = 0;
    if (offset_x > max_x) offset_x = max_x;
    if (offset_y > max_y) offset_y = max_y;
    t->scroll_offset_x = offset_x;
    t->scroll_offset_y = offset_y;
    textbox_invalidate(c);
    ui_unlock(&t->spinlock);
}

int draw_textbox(component_t *c, window_t *win)
{
    if (!c || !win || !win->buffer || !c->visible || !c->enabled ||
        !c->render || c->type != UI_TEXTBOX)
        return 0;
    textbox_data_t *t = (textbox_data_t *)c->data;
    if (!t || !t->ctx || !t->ctx->window ||
        !t->ctx->window->buffer)
        return 0;

    ui_lock(&t->spinlock);
    ui_lock(&win->lock);
    int view_width = textbox_view_width(c, t);
    int view_height = textbox_view_height(c, t);

    draw_rect_component(
        win->buffer, win->width, win->height,
        c->x, c->y, c->w, c->h,
        c->bg, c->border_color,
        t->border_radius, t->border_thickness
    );

    window_t *private_win = t->ctx->window;
    ui_lock(&private_win->lock);
    textbox_render_viewport(c, t, private_win, view_width, view_height);
    textbox_copy_view(
        win, private_win,
        0, 0,
        view_width, view_height,
        c->x + t->border_thickness,
        c->y + t->border_thickness
    );
    ui_unlock(&private_win->lock);

    int max_y = t->content_height - view_height;
    if (t->show_vertical_scrollbar) {
        int track_x = c->x + c->w - t->scrollbar_width;
        fill_rect(win->buffer, win->width, win->height,
                  track_x, c->y + t->border_thickness,
                  t->scrollbar_width, view_height, t->scrollbar_bg);
        if (max_y > 0) {
            int thumb, max_thumb, max_offset;
            textbox_get_thumb(view_height, t->content_height,
                              &thumb, &max_thumb, &max_offset,
                              t->char_height * 2);
            int thumb_y = max_offset > 0
                ? (int)(((int64_t)t->scroll_offset_y * max_thumb) / max_offset)
                : 0;
            fill_rect(win->buffer, win->width, win->height,
                      track_x, c->y + t->border_thickness + thumb_y,
                      t->scrollbar_width, thumb, t->scrollbar_fg);
        }
    }

    int max_x = t->content_width - view_width;
    if (t->show_horizontal_scrollbar) {
        int track_y = c->y + c->h - t->scrollbar_width;
        fill_rect(win->buffer, win->width, win->height,
                  c->x + t->border_thickness, track_y,
                  view_width, t->scrollbar_width, t->scrollbar_bg);
        if (max_x > 0) {
            int thumb, max_thumb, max_offset;
            textbox_get_thumb(view_width, t->content_width,
                              &thumb, &max_thumb, &max_offset,
                              t->char_height * 2);
            int thumb_x = max_offset > 0
                ? (int)(((int64_t)t->scroll_offset_x * max_thumb) / max_offset)
                : 0;
            fill_rect(win->buffer, win->width, win->height,
                      c->x + t->border_thickness + thumb_x, track_y,
                      thumb, t->scrollbar_width, t->scrollbar_fg);
        }
    }

    c->render = 0;
    c->dirty = 0;
    c->clear = 0;
    ui_unlock(&win->lock);
    ui_unlock(&t->spinlock);
    return 1;
}

int textbox_event(component_t *c, event_t ev)
{
    if (!c || !c->visible || !c->enabled ||
        c->type != UI_TEXTBOX || !c->ctx || !c->ctx->window)
        return 0;
    textbox_data_t *t = c->data;
    if (!t) return 0;

    ui_lock(&t->spinlock);
    int view_width = textbox_view_width(c, t);
    int view_height = textbox_view_height(c, t);
    int vertical_x = c->x + c->w - t->scrollbar_width;
    int horizontal_y = c->y + c->h - t->scrollbar_width;
    int viewport_x = c->x + t->border_thickness;
    int viewport_y = c->y + t->border_thickness;

    if (ev.type == EVENT_MOUSE_LEFT_DOWN) {
        if (!textbox_contains(c, ev.mouse.x, ev.mouse.y)) {
            ui_unlock(&t->spinlock);
            return 0;
        }

        if (t->show_vertical_scrollbar &&
            ev.mouse.x >= vertical_x &&
            ev.mouse.y >= viewport_y &&
            ev.mouse.y < viewport_y + view_height) {
            int thumb, max_thumb, max_offset;
            textbox_get_thumb(view_height, t->content_height,
                              &thumb, &max_thumb, &max_offset,
                              t->char_height * 2);
            if (max_offset > 0 && max_thumb > 0) {
                int thumb_y = (int)(((int64_t)t->scroll_offset_y * max_thumb) / max_offset);
                int mouse_y = ev.mouse.y - c->y - t->border_thickness;
                if (mouse_y >= thumb_y && mouse_y < thumb_y + thumb) {
                    t->dragging_vertical = 1;
                    t->drag_offset_y = mouse_y - thumb_y;
                } else {
                    int new_y = mouse_y - thumb / 2;
                    if (new_y < 0) new_y = 0;
                    if (new_y > max_thumb) new_y = max_thumb;
                    t->scroll_offset_y = (int)(((int64_t)new_y * max_offset) / max_thumb);
                }
            }
            textbox_invalidate(c);
            ui_unlock(&t->spinlock);
            return 1;
        }

        if (t->show_horizontal_scrollbar &&
            ev.mouse.y >= horizontal_y &&
            ev.mouse.x >= viewport_x &&
            ev.mouse.x < viewport_x + view_width) {
            int thumb, max_thumb, max_offset;
            textbox_get_thumb(view_width, t->content_width,
                              &thumb, &max_thumb, &max_offset,
                              t->char_height * 2);
            if (max_offset > 0 && max_thumb > 0) {
                int thumb_x = (int)(((int64_t)t->scroll_offset_x * max_thumb) / max_offset);
                int mouse_x = ev.mouse.x - c->x - t->border_thickness;
                if (mouse_x >= thumb_x && mouse_x < thumb_x + thumb) {
                    t->dragging_horizontal = 1;
                    t->drag_offset_x = mouse_x - thumb_x;
                } else {
                    int new_x = mouse_x - thumb / 2;
                    if (new_x < 0) new_x = 0;
                    if (new_x > max_thumb) new_x = max_thumb;
                    t->scroll_offset_x = (int)(((int64_t)new_x * max_offset) / max_thumb);
                }
            }
            textbox_invalidate(c);
            ui_unlock(&t->spinlock);
            return 1;
        }

        ui_unlock(&t->spinlock);
        return 1;
    }

    if (ev.type == EVENT_MOUSE_MOVE) {
        if (t->dragging_vertical) {
            int thumb, max_thumb, max_offset;
            textbox_get_thumb(view_height, t->content_height,
                              &thumb, &max_thumb, &max_offset,
                              t->char_height * 2);
            if (max_thumb > 0 && max_offset > 0) {
                int new_y = ev.mouse.y - c->y - t->border_thickness - t->drag_offset_y;
                if (new_y < 0) new_y = 0;
                if (new_y > max_thumb) new_y = max_thumb;
                t->scroll_offset_y = (int)(((int64_t)new_y * max_offset) / max_thumb);
            }
            textbox_invalidate(c);
            ui_unlock(&t->spinlock);
            return 1;
        }
        if (t->dragging_horizontal) {
            int thumb, max_thumb, max_offset;
            textbox_get_thumb(view_width, t->content_width,
                              &thumb, &max_thumb, &max_offset,
                              t->char_height * 2);
            if (max_thumb > 0 && max_offset > 0) {
                int new_x = ev.mouse.x - c->x - t->border_thickness - t->drag_offset_x;
                if (new_x < 0) new_x = 0;
                if (new_x > max_thumb) new_x = max_thumb;
                t->scroll_offset_x = (int)(((int64_t)new_x * max_offset) / max_thumb);
            }
            textbox_invalidate(c);
            ui_unlock(&t->spinlock);
            return 1;
        }
    }

    if (ev.type == EVENT_MOUSE_UP) {
        t->dragging_vertical = 0;
        t->dragging_horizontal = 0;
        ui_unlock(&t->spinlock);
        return 1;
    }

    ui_unlock(&t->spinlock);
    return 0;
}