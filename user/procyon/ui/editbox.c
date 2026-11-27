#include "include/components.h"
#include "include/editbox.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

#define EDITBOX_SCROLLBAR_WIDTH 14
#define EDITBOX_TAB_SIZE 4

static int editbox_find_line_start(editbox_data_t *e, int position);
static int editbox_find_line_end(editbox_data_t *e, int position);

static int editbox_view_width(component_t *c, editbox_data_t *e)
{
    int width = c->w - e->border_thickness * 2;

    if (e->show_vertical_scrollbar)
        width -= e->scrollbar_width;

    return width > 1 ? width : 1;
}

static int editbox_view_height(component_t *c, editbox_data_t *e)
{
    int height = c->h - e->border_thickness * 2;

    if (e->show_horizontal_scrollbar)
        height -= e->scrollbar_width;

    return height > 1 ? height : 1;
}

static char *editbox_expand_tabs(
    component_t *c,
    editbox_data_t *e,
    const char *text,
    size_t length
)
{
    if (!c || !e || (!text && length))
        return NULL;

    if (length > (SIZE_MAX - 1) / EDITBOX_TAB_SIZE)
        return NULL;

    size_t capacity = length * EDITBOX_TAB_SIZE + 1;
    char *output = malloc(capacity);

    if (!output)
        return NULL;

    size_t pos = 0;

    for (size_t i = 0; i < length; i++) {
        if (text[i] == '\t') {
            for (int j = 0; j < EDITBOX_TAB_SIZE; j++)
                output[pos++] = ' ';
        } else {
            output[pos++] = (e->input_type == EDITBOX_PASSWORD)
                ? '*' : text[i];
        }
    }

    output[pos] = '\0';
    return output;
}

static int editbox_measure_text(
    component_t *c,
    editbox_data_t *e,
    const char *text,
    size_t length
)
{
    if (!c || !e || (!text && length))
        return 0;

    char *expanded = editbox_expand_tabs(c, e, text, length);

    if (!expanded)
        return 0;

    size_t expanded_length = strlen(expanded);
    int fixed_char_width = e->font_size / 2;

    if (fixed_char_width < 1)
        fixed_char_width = 1;

    int width;

    if (e->char_width == fixed_char_width) {
        if (expanded_length > (size_t)INT_MAX / (size_t)fixed_char_width)
            width = INT_MAX;
        else
            width = (int)expanded_length * fixed_char_width;
    } else {
        width = text_get_string_width(
            &c->font,
            (float)c->font_size,
            expanded,
            (int)expanded_length
        );
    }

    free(expanded);
    return width > 0 ? width : 0;
}

static int editbox_line_width(
    component_t *c,
    editbox_data_t *e,
    int start,
    int end
)
{
    if (!c || !e || start < 0 || end <= start)
        return 0;

    return editbox_measure_text(
        c, e, e->text + start, (size_t)(end - start)
    );
}

static int editbox_horizontal_offset(component_t *c, editbox_data_t *e, int start, int end)
{
    int view_width = editbox_view_width(c, e);
    int line_width = editbox_line_width(c, e, start, end);

    if (line_width >= view_width)
        return 0;

    if (e->horizontal_align == EDITBOX_ALIGN_CENTER)
        return (view_width - line_width) / 2;

    if (e->horizontal_align == EDITBOX_ALIGN_RIGHT)
        return view_width - line_width;

    return 0;
}

static int editbox_vertical_offset(component_t *c, editbox_data_t *e)
{
    if (e->multiline)
        return 0;

    int text_height = e->char_height;
    int view_height = editbox_view_height(c, e);

    if (text_height >= view_height)
        return 0;

    if (e->vertical_align == EDITBOX_ALIGN_VCENTER)
        return (view_height - text_height) / 2;

    if (e->vertical_align == EDITBOX_ALIGN_BOTTOM)
        return view_height - text_height;

    return 0;
}

static void editbox_invalidate(component_t *c)
{
    if (!c)
        return;

    c->render = 1;
    c->dirty = 1;
}

static int editbox_contains(component_t *c, int x, int y)
{
    if (!c)
        return 0;

    return x >= c->x &&
           y >= c->y &&
           x < c->x + c->w &&
           y < c->y + c->h;
}

static int editbox_resize_buffer(editbox_data_t *e, int width, int height)
{
    if (!e || !e->ctx || !e->ctx->window)
        return -1;

    if (width < 1)
        width = 1;

    if (height < 1)
        height = 1;

    window_t *win = e->ctx->window;

    if (win->width == width && win->height == height)
        return 0;

    if ((size_t)width > SIZE_MAX / (size_t)height)
        return -1;

    size_t pixels = (size_t)width * (size_t)height;

    if (pixels > SIZE_MAX / sizeof(uint32_t))
        return -1;

    ui_lock(&win->lock);

    uint32_t *buffer = realloc(
        win->buffer,
        pixels * sizeof(uint32_t)
    );

    if (!buffer) {
        ui_unlock(&win->lock);
        return -1;
    }

    win->buffer = buffer;
    win->width = width;
    win->height = height;

    for (size_t i = 0; i < pixels; i++)
        buffer[i] = win->bg;

    win->invalid = 1;

    ui_unlock(&win->lock);

    return 0;
}

static int editbox_get_cursor_position(
    component_t *c,
    editbox_data_t *e,
    int *cursor_x,
    int *cursor_y
)
{
    if (!c || !e || !cursor_x || !cursor_y)
        return -1;

    int line_start = editbox_find_line_start(e, e->cursor_pos);
    int line_end = editbox_find_line_end(e, line_start);
    int x = editbox_measure_text(
        c,
        e,
        e->text + line_start,
        (size_t)(e->cursor_pos - line_start)
    );

    int y = 0;
    for (int i = 0; i < line_start; i++) {
        if (e->text[i] == '\n' && e->multiline)
            y += e->char_height;
    }

    x += editbox_horizontal_offset(c, e, line_start, line_end);
    y += editbox_vertical_offset(c, e);

    *cursor_x = x;
    *cursor_y = y;
    return 0;
}

static int editbox_rebuild(component_t *c, editbox_data_t *e)
{
    if (!c || !e || !e->ctx || !e->ctx->window)
        return -1;

    if (e->char_width <= 0 || e->char_height <= 0)
        return -1;

    int max_width = 0;
    int lines = 1;
    size_t line_start = 0;

    for (size_t i = 0; i <= e->text_length; i++) {
        if (i == e->text_length ||
            (e->text[i] == '\n' && e->multiline)) {
            int line_width = editbox_measure_text(
                c, e, e->text + line_start, i - line_start
            );

            if (line_width > max_width)
                max_width = line_width;

            if (i < e->text_length)
                lines++;

            line_start = i + 1;
        }
    }

    if (max_width > INT_MAX - c->font_size ||
        lines > INT_MAX / e->char_height)
        return -1;

    int content_width = max_width + c->font_size;
    int content_height = lines * e->char_height;

    int view_width = editbox_view_width(c, e);
    int view_height = editbox_view_height(c, e);

    if (content_width < view_width)
        content_width = view_width;

    if (content_height < view_height)
        content_height = view_height;

    e->content_width = content_width;
    e->content_height = content_height;

    if (editbox_resize_buffer(e, view_width, view_height) != 0)
        return -1;

    editbox_get_cursor_position(c, e, &e->cursor_x, &e->cursor_y);

    int max_x = e->content_width - view_width;
    int max_y = e->content_height - view_height;

    if (max_x < 0)
        max_x = 0;

    if (max_y < 0)
        max_y = 0;

    if (e->scroll_offset_x > max_x)
        e->scroll_offset_x = max_x;

    if (e->scroll_offset_y > max_y)
        e->scroll_offset_y = max_y;

    if (e->scroll_offset_x < 0)
        e->scroll_offset_x = 0;

    if (e->scroll_offset_y < 0)
        e->scroll_offset_y = 0;

    return 0;
}

static void editbox_ensure_cursor_visible(
    component_t *c,
    editbox_data_t *e
)
{
    int view_width = editbox_view_width(c, e);
    int view_height = editbox_view_height(c, e);

    if (e->cursor_x < e->scroll_offset_x)
        e->scroll_offset_x = e->cursor_x;

    if (e->cursor_x + 2 >
        e->scroll_offset_x + view_width)
        e->scroll_offset_x =
            e->cursor_x + 2 - view_width;

    if (e->cursor_y < e->scroll_offset_y)
        e->scroll_offset_y = e->cursor_y;

    if (e->cursor_y + e->char_height >
        e->scroll_offset_y + view_height)
        e->scroll_offset_y =
            e->cursor_y + e->char_height - view_height;

    int max_x = e->content_width - view_width;
    int max_y = e->content_height - view_height;

    if (max_x < 0)
        max_x = 0;

    if (max_y < 0)
        max_y = 0;

    if (e->scroll_offset_x > max_x)
        e->scroll_offset_x = max_x;

    if (e->scroll_offset_y > max_y)
        e->scroll_offset_y = max_y;

    if (e->scroll_offset_x < 0)
        e->scroll_offset_x = 0;

    if (e->scroll_offset_y < 0)
        e->scroll_offset_y = 0;
}

static void editbox_get_thumb(
    int viewport,
    int content,
    int *thumb_size,
    int *max_thumb_offset,
    int *max_content_offset,
    int min_thumb
)
{
    *thumb_size = viewport;
    *max_thumb_offset = 0;
    *max_content_offset = 0;

    if (viewport <= 0 || content <= viewport)
        return;

    int thumb = (int)(((int64_t)viewport * viewport) / content);

    if (min_thumb < 1)
        min_thumb = 1;

    if (thumb < min_thumb && viewport >= min_thumb)
        thumb = min_thumb;

    if (thumb > viewport)
        thumb = viewport;

    *thumb_size = thumb;
    *max_thumb_offset = viewport - thumb;
    *max_content_offset = content - viewport;
}

static void editbox_copy_view(
    window_t *dst,
    window_t *src,
    int source_x,
    int source_y,
    int width,
    int height,
    int dest_x,
    int dest_y
)
{
    if (!dst || !src || !dst->buffer || !src->buffer)
        return;

    for (int y = 0; y < height; y++) {
        int sy = source_y + y;
        int dy = dest_y + y;

        if (sy < 0 || sy >= src->height ||
            dy < 0 || dy >= dst->height)
            continue;

        for (int x = 0; x < width; x++) {
            int sx = source_x + x;
            int dx = dest_x + x;

            if (sx < 0 || sx >= src->width ||
                dx < 0 || dx >= dst->width)
                continue;

            dst->buffer[(size_t)dy * dst->width + dx] =
                src->buffer[(size_t)sy * src->width + sx];
        }
    }
}

static int editbox_validate_bytes(editbox_data_t *e, const char *text, size_t length)
{
    if (!e || (!text && length))
        return 0;

    for (size_t i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)text[i];

        if (e->input_type == EDITBOX_NUMBER) {
            if (ch < '0' || ch > '9')
                return 0;
            continue;
        }

        if (e->input_type == EDITBOX_PASSWORD) {
            if (ch < 32 || ch > 126)
                return 0;
            continue;
        }

        if (ch == '\n' || ch == '\r') {
            if (!e->multiline)
                return 0;
        } else if (ch == '\t') {
            continue;
        } else if (ch < 32 || ch > 126) {
            return 0;
        }
    }

    return 1;
}

static int editbox_reserve(editbox_data_t *e, size_t required)
{
    if (required <= e->text_capacity)
        return 0;

    size_t capacity = e->text_capacity ? e->text_capacity : 64;

    while (capacity < required) {
        if (capacity > SIZE_MAX / 2)
            return -1;

        capacity *= 2;
    }

    char *text = realloc(e->text, capacity);

    if (!text)
        return -1;

    e->text = text;
    e->text_capacity = capacity;

    return 0;
}

static int editbox_insert_bytes(
    component_t *c,
    editbox_data_t *e,
    const char *text,
    size_t length
)
{
    if (!c || !e || !text)
        return -1;

    if (length == 0)
        return 0;

    if (!editbox_validate_bytes(e, text, length))
        return -1;

    if (e->cursor_pos < 0 ||
        (size_t)e->cursor_pos > e->text_length)
        return -1;

    if (length > (size_t)INT_MAX - (size_t)e->cursor_pos)
        return -1;

    if (e->text_length > SIZE_MAX - length - 1)
        return -1;

    size_t new_length = e->text_length + length;

    if (editbox_reserve(e, new_length + 1) != 0)
        return -1;

    memmove(
        e->text + e->cursor_pos + length,
        e->text + e->cursor_pos,
        e->text_length - (size_t)e->cursor_pos + 1
    );

    memcpy(
        e->text + e->cursor_pos,
        text,
        length
    );

    e->text_length = new_length;
    e->cursor_pos += (int)length;

    if (editbox_rebuild(c, e) != 0)
        return -1;

    editbox_ensure_cursor_visible(c, e);
    editbox_invalidate(c);

    return 0;
}

static int editbox_delete_before(component_t *c, editbox_data_t *e)
{
    if (e->cursor_pos <= 0)
        return 0;

    memmove(
        e->text + e->cursor_pos - 1,
        e->text + e->cursor_pos,
        e->text_length - (size_t)e->cursor_pos + 1
    );

    e->cursor_pos--;
    e->text_length--;

    if (editbox_rebuild(c, e) != 0)
        return -1;

    editbox_ensure_cursor_visible(c, e);
    editbox_invalidate(c);

    return 0;
}

static int editbox_delete_at(component_t *c, editbox_data_t *e)
{
    if ((size_t)e->cursor_pos >= e->text_length)
        return 0;

    memmove(
        e->text + e->cursor_pos,
        e->text + e->cursor_pos + 1,
        e->text_length - (size_t)e->cursor_pos
    );

    e->text_length--;

    if (editbox_rebuild(c, e) != 0)
        return -1;

    editbox_ensure_cursor_visible(c, e);
    editbox_invalidate(c);

    return 0;
}

static int editbox_find_line_start(editbox_data_t *e, int position)
{
    if (position < 0)
        position = 0;

    if (position > (int)e->text_length)
        position = (int)e->text_length;

    while (position > 0) {
        if (e->text[position - 1] == '\n' && e->multiline)
            break;

        position--;
    }

    return position;
}

static int editbox_find_line_end(editbox_data_t *e, int position)
{
    while (position < (int)e->text_length) {
        if (e->text[position] == '\n' && e->multiline)
            break;

        position++;
    }

    return position;
}

static int editbox_index_at_position(
    component_t *c,
    editbox_data_t *e,
    int mouse_x,
    int mouse_y
)
{
    int x = mouse_x - c->x - e->border_thickness +
            e->scroll_offset_x;

    int y = mouse_y - c->y - e->border_thickness +
            e->scroll_offset_y - editbox_vertical_offset(c, e);

    int line = y / e->char_height;

    if (line < 0)
        line = 0;

    int start = 0;

    for (int i = 0; i < line; i++) {
        int end = editbox_find_line_end(e, start);

        if (end >= (int)e->text_length)
            return end;

        start = end + 1;
    }

    int end = editbox_find_line_end(e, start);
    x -= editbox_horizontal_offset(c, e, start, end);

    if (x <= 0)
        return start;

    int previous_width = 0;

    for (int i = start; i < end; i++) {
        int current_width = editbox_measure_text(
            c, e, e->text + start, (size_t)(i - start + 1)
        );

        int advance = current_width - previous_width;

        if (x < previous_width + advance / 2)
            return i;

        if (x < current_width)
            return i + 1;

        previous_width = current_width;
    }

    return end;
}

static int editbox_index_for_line_x(
    component_t *c,
    editbox_data_t *e,
    int start,
    int end,
    int target_x
)
{
    int previous_width = 0;

    for (int i = start; i < end; i++) {
        int current_width = editbox_measure_text(
            c, e, e->text + start, (size_t)(i - start + 1)
        );

        int advance = current_width - previous_width;

        if (target_x < previous_width + advance / 2)
            return i;

        if (target_x < current_width)
            return i + 1;

        previous_width = current_width;
    }

    return end;
}

static void editbox_move_vertical(
    component_t *c,
    editbox_data_t *e,
    int direction
)
{
    if (!e->multiline)
        return;

    int line_start = editbox_find_line_start(e, e->cursor_pos);
    int line_end = editbox_find_line_end(e, line_start);
    int old_x = e->cursor_x -
        editbox_horizontal_offset(c, e, line_start, line_end);

    if (direction < 0) {
        if (line_start == 0)
            return;

        int previous_end = line_start - 1;
        int previous_start = editbox_find_line_start(e, previous_end);

        e->cursor_pos = editbox_index_for_line_x(
            c, e, previous_start, previous_end,
            old_x
        );
    } else {
        int line_end = editbox_find_line_end(e, e->cursor_pos);

        if (line_end >= (int)e->text_length)
            return;

        int next_start = line_end + 1;
        int next_end = editbox_find_line_end(e, next_start);

        e->cursor_pos = editbox_index_for_line_x(
            c, e, next_start, next_end,
            old_x
        );
    }

    editbox_get_cursor_position(c, e, &e->cursor_x, &e->cursor_y);
    editbox_ensure_cursor_visible(c, e);
    editbox_invalidate(c);
}

static void editbox_on_clear(component_t *c)
{
    if (!c || c->type != UI_EDITTEXT)
        return;

    editbox_set_text(c, "");
}

static void editbox_on_enable(component_t *c)
{
    if (!c || c->type != UI_EDITTEXT)
        return;

    editbox_data_t *e = c->data;
    if (!e)
        return;

    ui_lock(&e->spinlock);
    e->background_render = 1;
    editbox_invalidate(c);
    ui_unlock(&e->spinlock);
}

static void editbox_on_disable(component_t *c)
{
    if (!c || c->type != UI_EDITTEXT)
        return;

    editbox_data_t *e = c->data;
    if (!e)
        return;

    ui_lock(&e->spinlock);

    e->focused = 0;
    e->dragging_vertical = 0;
    e->dragging_horizontal = 0;
    e->drag_offset_x = 0;
    e->drag_offset_y = 0;

    editbox_invalidate(c);

    ui_unlock(&e->spinlock);
}

static void editbox_on_destroy(component_t *c)
{
    if (!c)
        return;

    editbox_data_t *e = c->data;
    if (!e)
        return;

    if (e->ctx) {
        if (e->ctx->window) {
            free(e->ctx->window->buffer);
            e->ctx->window->buffer = NULL;
            free(e->ctx->window);
            e->ctx->window = NULL;
        }

        free(e->ctx->components);
        e->ctx->components = NULL;
        free(e->ctx);
        e->ctx = NULL;
    }

    free(e->text);
    e->text = NULL;
    e->text_length = 0;
    e->text_capacity = 0;

    free(e);
    c->data = NULL;
}

static component_t *editbox_create_internal(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    int font_size,
    editbox_type_t type,
    const char *font_path,
    int simple
)
{
    if (!ctx || !ctx->window || !ctx->window->font ||
        w <= 0 || h <= 0 || font_size <= 0 ||
        type < EDITBOX_TEXT || type > EDITBOX_NUMBER)
        return NULL;

    component_t *c = calloc(1, sizeof(component_t));
    if (!c)
        return NULL;

    editbox_data_t *e = calloc(1, sizeof(editbox_data_t));
    if (!e) {
        free(c);
        return NULL;
    }

    e->ctx = calloc(1, sizeof(ui_context_t));
    if (!e->ctx) {
        free(e);
        free(c);
        return NULL;
    }

    e->ctx->fd = ctx->fd;
    e->ctx->suppress_server_notify = 1;
    e->input_type = type;
    e->font_size = font_size;
    e->horizontal_align = EDITBOX_ALIGN_LEFT;
    e->vertical_align = EDITBOX_ALIGN_TOP;

    e->border_radius = 5;
    e->border_thickness = 1;
    e->background_render = 1;

    e->scrollbar_width = EDITBOX_SCROLLBAR_WIDTH;
    e->show_vertical_scrollbar = 1;
    e->show_horizontal_scrollbar = 1;
    e->scrollbar_bg = argb(255, 38, 40, 46);
    e->scrollbar_fg = argb(255, 120, 125, 135);

    e->multiline = 0;
    e->focused = 0;
    e->hide_input = 0;

    e->text_capacity = 64;
    e->text = calloc(1, e->text_capacity);
    if (!e->text) {
        free(e->ctx);
        free(e);
        free(c);
        return NULL;
    }

    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;

    c->type = UI_EDITTEXT;
    c->visible = 1;
    c->enabled = 1;
    c->render = 1;
    c->dirty = 1;
    c->ctx = ctx;
    c->data = e;

    c->bg = argb(255, 30, 32, 38);
    c->fg = argb(255, 235, 235, 235);
    c->border_color = argb(255, 80, 85, 95);
    c->font_size = font_size;

    unsigned char *font_buffer = NULL;
    stbtt_fontinfo *font = malloc(sizeof(stbtt_fontinfo));

    if (font) {
        memset(font, 0, sizeof(stbtt_fontinfo));

        if (stbtt_font_init(
                font,
                font_path,
                &font_buffer
            ) == 0) {
            c->free_font_buffer = font_buffer;
            c->font = *font;
        } else {
            free(font_buffer);
            font_buffer = NULL;
            c->font = *(stbtt_fontinfo *)ctx->window->font;
        }

        free(font);
    } else {
        c->font = *(stbtt_fontinfo *)ctx->window->font;
    }

    if (simple) {
        e->char_width = font_size / 2;
    } else {
        e->char_width = text_get_string_width(
            &c->font,
            (float)c->font_size,
            " ",
            1
        );
    }

    e->char_height = c->font_size;

    if (e->char_width <= 0)
        e->char_width = 1;
    if (e->char_height <= 0)
        e->char_height = 1;

    e->ctx->window = component_window_create(
        ctx->window,
        editbox_view_width(c, e),
        editbox_view_height(c, e),
        c->bg,
        0
    );

    if (!e->ctx->window)
        goto fail;

    build_font_cache(&c->font, c->font_size, &c->font_cache);

    c->on_draw = draw_editbox;
    c->on_event = editbox_event;

    c->on_clear = editbox_on_clear;
    c->on_enable = editbox_on_enable;
    c->on_disable = editbox_on_disable;
    c->on_destroy = editbox_on_destroy;

    if (editbox_rebuild(c, e) != 0)
        goto fail;

    if (ui_add_component(ctx, c) != 0)
        goto fail;

    return c;

fail:
    if (e->ctx && e->ctx->window) {
        free(e->ctx->window->buffer);
        e->ctx->window->buffer = NULL;
        free(e->ctx->window);
        e->ctx->window = NULL;
    }

    if (e->ctx) {
        free(e->ctx->components);
        free(e->ctx);
    }

    free(e->text);
    free(e);
    free(c->free_font_buffer);
    free(c);

    return NULL;
}

component_t *create_editbox(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    int font_size,
    editbox_type_t type
)
{
    return editbox_create_internal(
        ctx, x, y, w, h, font_size, type,
        "usr/share/fonts/Ubuntu.ttf", 0
    );
}

component_t *create_simple_editbox(
    ui_context_t *ctx,
    int x,
    int y,
    int w,
    int h,
    int font_size,
    editbox_type_t type
)
{
    return editbox_create_internal(
        ctx, x, y, w, h, font_size, type,
        "usr/share/fonts/UbuntuMono.ttf", 1
    );
}

void editbox_set_alignment(
    component_t *c,
    editbox_horizontal_align_t horizontal,
    editbox_vertical_align_t vertical
)
{
    if (!c || c->type != UI_EDITTEXT)
        return;

    editbox_data_t *e = c->data;
    if (!e)
        return;

    if (horizontal < EDITBOX_ALIGN_LEFT || horizontal > EDITBOX_ALIGN_RIGHT)
        return;

    if (vertical < EDITBOX_ALIGN_TOP || vertical > EDITBOX_ALIGN_BOTTOM)
        return;

    ui_lock(&e->spinlock);
    e->horizontal_align = horizontal;
    e->vertical_align = vertical;
    editbox_rebuild(c, e);
    editbox_get_cursor_position(c, e, &e->cursor_x, &e->cursor_y);
    editbox_ensure_cursor_visible(c, e);
    editbox_invalidate(c);
    ui_unlock(&e->spinlock);
}

void editbox_set_vertical_alignment(
    component_t *c,
    editbox_vertical_align_t vertical
)
{
    if (!c || c->type != UI_EDITTEXT)
        return;

    editbox_data_t *e = c->data;
    if (!e)
        return;

    if (vertical < EDITBOX_ALIGN_TOP || vertical > EDITBOX_ALIGN_BOTTOM)
        return;

    ui_lock(&e->spinlock);

    if (e->multiline) {
        ui_unlock(&e->spinlock);
        return;
    }

    e->vertical_align = vertical;

    editbox_rebuild(c, e);
    editbox_get_cursor_position(c, e, &e->cursor_x, &e->cursor_y);
    editbox_ensure_cursor_visible(c, e);
    editbox_invalidate(c);

    ui_unlock(&e->spinlock);
}

void editbox_set_text(component_t *c, const char *text)
{
    if (!c || c->type != UI_EDITTEXT)
        return;

    editbox_data_t *e = c->data;

    if (!e || !text)
        return;

    ui_lock(&e->spinlock);

    size_t length = strlen(text);

    if (length > INT_MAX) {
        ui_unlock(&e->spinlock);
        return;
    }

    char *normalized = malloc(length + 1);

    if (!normalized) {
        ui_unlock(&e->spinlock);
        return;
    }

    memcpy(normalized, text, length + 1);

    if (e->input_type == EDITBOX_TEXT && !e->multiline) {
        for (size_t i = 0; i < length; i++) {
            if (normalized[i] == '\n' || normalized[i] == '\r')
                normalized[i] = ' ';
        }
    }

    if (!editbox_validate_bytes(e, normalized, length)) {
        free(normalized);
        ui_unlock(&e->spinlock);
        return;
    }

    if (editbox_reserve(e, length + 1) != 0) {
        free(normalized);
        ui_unlock(&e->spinlock);
        return;
    }

    memcpy(e->text, normalized, length + 1);
    free(normalized);

    e->text_length = length;
    e->cursor_pos = (int)e->text_length;

    editbox_rebuild(c, e);
    editbox_ensure_cursor_visible(c, e);
    editbox_invalidate(c);

    ui_unlock(&e->spinlock);
}

void editbox_set_multiline(component_t *c, int enabled)
{
    if (!c || c->type != UI_EDITTEXT)
        return;

    editbox_data_t *e = c->data;

    if (!e)
        return;

    ui_lock(&e->spinlock);

    e->multiline = (e->input_type == EDITBOX_TEXT) ? !!enabled : 0;

    if (!e->multiline) {
        for (size_t i = 0; i < e->text_length; i++) {
            if (e->text[i] == '\n' || e->text[i] == '\r')
                e->text[i] = ' ';
        }
    }

    e->cursor_pos = (int)e->text_length;

    editbox_rebuild(c, e);
    editbox_ensure_cursor_visible(c, e);
    editbox_invalidate(c);

    ui_unlock(&e->spinlock);
}

void editbox_set_scrollbars(
    component_t *c,
    int vertical,
    int horizontal
)
{
    if (!c || c->type != UI_EDITTEXT)
        return;

    editbox_data_t *e = c->data;

    if (!e)
        return;

    ui_lock(&e->spinlock);

    e->show_vertical_scrollbar = !!vertical;
    e->show_horizontal_scrollbar = !!horizontal;

    if (!vertical)
        e->scroll_offset_y = 0;

    if (!horizontal)
        e->scroll_offset_x = 0;

    editbox_rebuild(c, e);
    editbox_ensure_cursor_visible(c, e);
    editbox_invalidate(c);

    ui_unlock(&e->spinlock);
}

void editbox_set_offset(
    component_t *c,
    int offset_x,
    int offset_y
)
{
    if (!c || c->type != UI_EDITTEXT)
        return;

    editbox_data_t *e = c->data;

    if (!e)
        return;

    ui_lock(&e->spinlock);

    e->scroll_offset_x = offset_x;
    e->scroll_offset_y = offset_y;

    int max_x = e->content_width - editbox_view_width(c, e);
    int max_y = e->content_height - editbox_view_height(c, e);

    if (max_x < 0)
        max_x = 0;

    if (max_y < 0)
        max_y = 0;

    if (e->scroll_offset_x < 0)
        e->scroll_offset_x = 0;

    if (e->scroll_offset_y < 0)
        e->scroll_offset_y = 0;

    if (e->scroll_offset_x > max_x)
        e->scroll_offset_x = max_x;

    if (e->scroll_offset_y > max_y)
        e->scroll_offset_y = max_y;

    editbox_invalidate(c);

    ui_unlock(&e->spinlock);
}

int editbox_write(component_t *c, const char *text)
{
    if (!c || c->type != UI_EDITTEXT || !text)
        return -1;

    editbox_data_t *e = c->data;

    if (!e)
        return -1;

    ui_lock(&e->spinlock);

    size_t length = strlen(text);

    if (!editbox_validate_bytes(e, text, length)) {
        ui_unlock(&e->spinlock);
        return -1;
    }

    int result = editbox_insert_bytes(c, e, text, length);

    ui_unlock(&e->spinlock);

    return result;
}

static int editbox_render_viewport(
    component_t *c,
    editbox_data_t *e,
    window_t *win
)
{
    if (!c || !e || !win || !win->buffer)
        return -1;

    for (int y = 0; y < win->height; y++) {
        for (int x = 0; x < win->width; x++)
            win->buffer[(size_t)y * win->width + x] = win->bg;
    }

    int y = editbox_vertical_offset(c, e) - e->scroll_offset_y;
    size_t start = 0;

    while (start < e->text_length) {
        size_t end = start;

        while (end < e->text_length &&
               !(e->text[end] == '\n' && e->multiline))
            end++;

        if (y + e->char_height > 0 && y < win->height && end > start) {
            char *line = editbox_expand_tabs(
                c, e, e->text + start, end - start
            );

            if (!line)
                return -1;

            int line_x = editbox_horizontal_offset(
                c, e, (int)start, (int)end
            ) - e->scroll_offset_x;

            const char *visible_line = line;
            int draw_x = line_x;

            /*
             * draw_text_cached recebe coordenadas dentro do buffer.
             * Evita enviar X negativo: descarta os caracteres que
             * ficaram totalmente antes da margem esquerda.
             */
            while (*visible_line && draw_x < 0) {
                int char_width = text_get_string_width(
                    &c->font,
                    (float)c->font_size,
                    visible_line,
                    1
                );

                if (char_width < 1)
                    char_width = 1;

                draw_x += char_width;
                visible_line++;
            }

            if (*visible_line && draw_x < win->width) {
                draw_text_cached(
                    &c->font_cache,
                    win->buffer,
                    win->width,
                    win->height,
                    visible_line,
                    draw_x,
                    y,
                    win->width - draw_x,
                    c->fg
                );
            }

            free(line);
        }

        y += e->char_height;
        start = end + 1;
    }

    return 0;
}

int draw_editbox(component_t *c, window_t *win)
{
    if (!c || !win || !c->visible || !c->enabled || !c->render)
        return 0;

    editbox_data_t *e = c->data;

    if (!e || !e->ctx || !e->ctx->window)
        return 0;

    ui_lock(&e->spinlock);
    ui_lock(&win->lock);

    int view_width = editbox_view_width(c, e);
    int view_height = editbox_view_height(c, e);

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
        e->border_radius,
        e->border_thickness
    );

    window_t *private_win = e->ctx->window;

    ui_lock(&private_win->lock);

    editbox_render_viewport(c, e, private_win);

    editbox_copy_view(
        win,
        private_win,
        0,
        0,
        view_width,
        view_height,
        c->x + e->border_thickness,
        c->y + e->border_thickness
    );

    ui_unlock(&private_win->lock);

    int max_y = e->content_height - view_height;

    if (e->show_vertical_scrollbar) {
        int track_x = c->x + c->w - e->scrollbar_width;

        fill_rect(
            win->buffer,
            win->width,
            win->height,
            track_x,
            c->y + e->border_thickness,
            e->scrollbar_width,
            view_height,
            e->scrollbar_bg //argb(255, 38, 40, 46)
        );

        if (max_y > 0) {
            int thumb;
            int max_thumb;
            int max_offset;

            editbox_get_thumb(
                view_height,
                e->content_height,
                &thumb,
                &max_thumb,
                &max_offset,
                e->char_height * 2
            );

            int thumb_y = 0;

            if (max_offset > 0) {
                thumb_y = (int)(
                    ((int64_t)e->scroll_offset_y * max_thumb) /
                    max_offset
                );
            }

            fill_rect(
                win->buffer,
                win->width,
                win->height,
                track_x,
                c->y + e->border_thickness + thumb_y,
                e->scrollbar_width,
                thumb,
                e->scrollbar_fg// argb(255, 120, 125, 135)
            );
        }
    }

    int max_x = e->content_width - view_width;

    if (e->show_horizontal_scrollbar) {
        int track_y = c->y + c->h - e->scrollbar_width;

        fill_rect(
            win->buffer,
            win->width,
            win->height,
            c->x + e->border_thickness,
            track_y,
            view_width,
            e->scrollbar_width,
            e->scrollbar_bg //argb(255, 38, 40, 46)
        );

        if (max_x > 0) {
            int thumb;
            int max_thumb;
            int max_offset;

            editbox_get_thumb(
                view_width,
                e->content_width,
                &thumb,
                &max_thumb,
                &max_offset,
                e->char_width * 2
            );

            int thumb_x = 0;

            if (max_offset > 0) {
                thumb_x = (int)(
                    ((int64_t)e->scroll_offset_x * max_thumb) /
                    max_offset
                );
            }

            fill_rect(
                win->buffer,
                win->width,
                win->height,
                c->x + e->border_thickness + thumb_x,
                track_y,
                thumb,
                e->scrollbar_width,
                e->scrollbar_fg //argb(255, 120, 125, 135)
            );
        }
    }
    
    if (e->focused && !e->hide_input) {
        int cursor_x = c->x + e->border_thickness +
                       e->cursor_x - e->scroll_offset_x;

        int cursor_y = c->y + e->border_thickness +
                       e->cursor_y - e->scroll_offset_y;

        if (cursor_x >= c->x &&
            cursor_x < c->x + e->border_thickness + view_width &&
            cursor_y >= c->y &&
            cursor_y + e->char_height <= c->y + c->h) {

            fill_rect(
                win->buffer,
                win->width,
                win->height,
                cursor_x,
                cursor_y,
                2,
                e->char_height,
                c->fg
            );
        }
    }

    c->render = 0;
    c->dirty = 0;
    c->clear = 0;

    ui_unlock(&win->lock);
    ui_unlock(&e->spinlock);

    return 1;
}

int editbox_event(component_t *c, event_t ev)
{
    if (!c || !c->visible || !c->enabled ||
        c->type != UI_EDITTEXT)
        return 0;

    editbox_data_t *e = c->data;

    if (!e)
        return 0;

    ui_lock(&e->spinlock);

    int view_width = editbox_view_width(c, e);
    int view_height = editbox_view_height(c, e);

    int vertical_x = c->x + c->w - e->scrollbar_width;
    int horizontal_y = c->y + c->h - e->scrollbar_width;

    if (ev.type == EVENT_MOUSE_LEFT_DOWN) {
        if (!editbox_contains(c, ev.mouse.x, ev.mouse.y)) {
            if (e->focused) {
                e->focused = 0;
                editbox_invalidate(c);
            }

            ui_unlock(&e->spinlock);
            return 0;
        }

        e->focused = 1;

        if (e->show_vertical_scrollbar &&
            ev.mouse.x >= vertical_x &&
            ev.mouse.y < horizontal_y) {

            int thumb;
            int max_thumb;
            int max_offset;

            editbox_get_thumb(
                view_height,
                e->content_height,
                &thumb,
                &max_thumb,
                &max_offset,
                e->char_height * 2
            );

            if (max_offset > 0 && max_thumb > 0) {
                int thumb_y = (int)(
                    ((int64_t)e->scroll_offset_y * max_thumb) /
                    max_offset
                );

                int mouse_y = ev.mouse.y - c->y -
                              e->border_thickness;

                if (mouse_y >= thumb_y &&
                    mouse_y < thumb_y + thumb) {
                    e->dragging_vertical = 1;
                    e->drag_offset_y = mouse_y - thumb_y;
                } else {
                    int new_y = mouse_y - thumb / 2;

                    if (new_y < 0)
                        new_y = 0;

                    if (new_y > max_thumb)
                        new_y = max_thumb;

                    e->scroll_offset_y = (int)(
                        ((int64_t)new_y * max_offset) / max_thumb
                    );
                }
            }

            editbox_invalidate(c);
            ui_unlock(&e->spinlock);
            return 1;
        }

        if (e->show_horizontal_scrollbar &&
            ev.mouse.y >= horizontal_y) {

            int thumb;
            int max_thumb;
            int max_offset;

            editbox_get_thumb(
                view_width,
                e->content_width,
                &thumb,
                &max_thumb,
                &max_offset,
                e->char_width * 2
            );

            if (max_offset > 0 && max_thumb > 0) {
                int thumb_x = (int)(
                    ((int64_t)e->scroll_offset_x * max_thumb) /
                    max_offset
                );

                int mouse_x = ev.mouse.x - c->x -
                              e->border_thickness;

                if (mouse_x >= thumb_x &&
                    mouse_x < thumb_x + thumb) {
                    e->dragging_horizontal = 1;
                    e->drag_offset_x = mouse_x - thumb_x;
                } else {
                    int new_x = mouse_x - thumb / 2;

                    if (new_x < 0)
                        new_x = 0;

                    if (new_x > max_thumb)
                        new_x = max_thumb;

                    e->scroll_offset_x = (int)(
                        ((int64_t)new_x * max_offset) / max_thumb
                    );
                }
            }

            editbox_invalidate(c);
            ui_unlock(&e->spinlock);
            return 1;
        }

        e->cursor_pos = editbox_index_at_position(
            c,
            e,
            ev.mouse.x,
            ev.mouse.y
        );

        editbox_get_cursor_position(c, e, &e->cursor_x, &e->cursor_y);
        editbox_ensure_cursor_visible(c, e);
        editbox_invalidate(c);

        ui_unlock(&e->spinlock);
        return 1;
    }

    if (ev.type == EVENT_MOUSE_MOVE) {
        if (e->dragging_vertical) {
            int thumb;
            int max_thumb;
            int max_offset;

            editbox_get_thumb(
                view_height,
                e->content_height,
                &thumb,
                &max_thumb,
                &max_offset,
                e->char_height * 2
            );

            if (max_thumb > 0 && max_offset > 0) {
                int new_y = ev.mouse.y - c->y -
                            e->border_thickness - e->drag_offset_y;

                if (new_y < 0)
                    new_y = 0;

                if (new_y > max_thumb)
                    new_y = max_thumb;

                e->scroll_offset_y = (int)(
                    ((int64_t)new_y * max_offset) / max_thumb
                );
            }

            editbox_invalidate(c);
            ui_unlock(&e->spinlock);
            return 1;
        }

        if (e->dragging_horizontal) {
            int thumb;
            int max_thumb;
            int max_offset;

            editbox_get_thumb(
                view_width,
                e->content_width,
                &thumb,
                &max_thumb,
                &max_offset,
                e->char_width * 2
            );

            if (max_thumb > 0 && max_offset > 0) {
                int new_x = ev.mouse.x - c->x -
                            e->border_thickness - e->drag_offset_x;

                if (new_x < 0)
                    new_x = 0;

                if (new_x > max_thumb)
                    new_x = max_thumb;

                e->scroll_offset_x = (int)(
                    ((int64_t)new_x * max_offset) / max_thumb
                );
            }

            editbox_invalidate(c);
            ui_unlock(&e->spinlock);
            return 1;
        }
    }

    if (ev.type == EVENT_MOUSE_UP) {
        e->dragging_vertical = 0;
        e->dragging_horizontal = 0;

        ui_unlock(&e->spinlock);
        return 1;
    }

    if (ev.type == EVENT_FOCUS_LOST) {
        e->focused = 0;
        e->dragging_vertical = 0;
        e->dragging_horizontal = 0;

        editbox_invalidate(c);

        ui_unlock(&e->spinlock);
        return 1;
    }

    if (ev.type == EVENT_FOCUS_GAINED) {
        e->focused = 1;
        editbox_invalidate(c);

        ui_unlock(&e->spinlock);
        return 1;
    }

    if (ev.type == EVENT_TEXT_INPUT && e->focused) {
        int codepoint = ev.text.codepoint;

        if (codepoint >= 32 && codepoint <= 126) {
            char ch = (char)codepoint;

            int result = editbox_insert_bytes(c, e, &ch, 1);

            ui_unlock(&e->spinlock);
            return result == 0;
        }

        ui_unlock(&e->spinlock);
        return 0;
    }

    if (ev.type == EVENT_KEY_DOWN && e->focused) {
        int key = ev.key.keycode;

        switch (key) {
            case KEY_BACKSPACE:
                editbox_delete_before(c, e);
                break;

            case KEY_DELETE:
                editbox_delete_at(c, e);
                break;

            case KEY_LEFT:

                if (e->cursor_pos > 0)
                    e->cursor_pos--;

                editbox_get_cursor_position(c, e, &e->cursor_x, &e->cursor_y);
                editbox_ensure_cursor_visible(c, e);
                editbox_invalidate(c);
                break;

            case KEY_RIGHT:
                if ((size_t)e->cursor_pos < e->text_length)
                    e->cursor_pos++;

                editbox_get_cursor_position(c, e, &e->cursor_x, &e->cursor_y);
                editbox_ensure_cursor_visible(c, e);
                editbox_invalidate(c);
                break;

            case KEY_UP:
                editbox_move_vertical(c, e, -1);
                break;

            case KEY_DOWN:
                editbox_move_vertical(c, e, 1);
                break;

            case KEY_HOME:
                e->cursor_pos = editbox_find_line_start(e, e->cursor_pos);
                editbox_get_cursor_position(c, e, &e->cursor_x, &e->cursor_y);
                editbox_ensure_cursor_visible(c, e);
                editbox_invalidate(c);
                break;

            case KEY_END:
                e->cursor_pos = editbox_find_line_end(e, e->cursor_pos);
                editbox_get_cursor_position(c, e, &e->cursor_x, &e->cursor_y);
                editbox_ensure_cursor_visible(c, e);
                editbox_invalidate(c);
                break;

            case KEY_ENTER:
                if (e->input_type == EDITBOX_TEXT && e->multiline)
                    editbox_insert_bytes(c, e, "\n", 1);
                break;

            case KEY_TAB:
                if (e->input_type == EDITBOX_TEXT)
                    editbox_insert_bytes(c, e, "\t", 1);
                break;

            default:
                ui_unlock(&e->spinlock);
                return 0;
        }

        ui_unlock(&e->spinlock);
        return 1;
    }

    ui_unlock(&e->spinlock);
    return 0;
}