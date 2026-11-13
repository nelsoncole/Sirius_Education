#include "text.h"
#include "utf8_decode.h"
#include "procyon_file.h"
#include "window.h"
#include "pixel.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "ported/stb_truetype.h"

extern unsigned long buffer_ttf;

static int text_is_space(uint32_t codepoint)
{
    return codepoint == ' '  || codepoint == '\t' ||
           codepoint == '\n' || codepoint == '\v' ||
           codepoint == '\f' || codepoint == '\r';
}

static uint32_t text_next_codepoint(const char **cursor)
{
    uint32_t codepoint;
    int consumed = utf8_decode(*cursor, &codepoint);

    if (consumed <= 0) {
        if (**cursor == '\0')
            return 0;

        ++(*cursor);
        return '?';
    }

    *cursor += consumed;
    return codepoint;
}

static int font_get_advance(const stbtt_fontinfo *font, uint32_t codepoint, float scale)
{
    int advance_width;

    stbtt_GetCodepointHMetrics(font, (int)codepoint,
                               &advance_width, NULL);

    return (int)(advance_width * scale);
}

static void text_draw_codepoint(stbtt_fontinfo *font, uint32_t codepoint, float scale,
                               int x, int baseline, unsigned int color,
                               window_t *win)
{
    int bitmap_width;
    int bitmap_height;
    int x_offset;
    int y_offset;

    unsigned char *bitmap = stbtt_GetCodepointBitmap(
        font, 0.0f, scale, (int)codepoint,
        &bitmap_width, &bitmap_height,
        &x_offset, &y_offset);

    if (bitmap == NULL)
        return;

    for (int row = 0; row < bitmap_height; ++row) {
        for (int column = 0; column < bitmap_width; ++column) {

            int pixel_x = x + column + x_offset;
            int pixel_y = baseline + row + y_offset;

            if (pixel_x < 0 || pixel_x >= win->width ||
                pixel_y < 0 || pixel_y >= win->height)
                continue;

            unsigned char coverage =
                bitmap[row * bitmap_width + column];

            unsigned int background = get_pixel(
                pixel_x, pixel_y,
                win->width, win->height,
                win->buffer);

            unsigned int blended_color =
                blend_color(background, color, coverage);

            draw_pixel(pixel_x, pixel_y,
                       win->width, win->height,
                       blended_color, win->buffer);
        }
    }

    stbtt_FreeBitmap(bitmap, NULL);
}

int stbtt_font_init(stbtt_fontinfo *font, const char *path, unsigned char **font_buffer_out)
{
    size_t file_size;
    unsigned char *font_buffer;

    if (font == NULL || path == NULL || font_buffer_out == NULL)
        return 2;

    *font_buffer_out = NULL;

    font_buffer = procyon_upload_file(path, &file_size);

    if (font_buffer == NULL)
        return 2;

    *font_buffer_out = font_buffer;

    if (!stbtt_InitFont(font, font_buffer, 0))
        return 1;

    return 0;
}

int draw_text(stbtt_fontinfo font, const char *text, int x, int y,
              int max_width, unsigned int color, int font_size, window_t *win)
{
    if (text == NULL || win == NULL || font_size <= 0)
        return 0;

    float scale = stbtt_ScaleForPixelHeight(&font, font_size);

    int ascent;
    int descent;
    int line_gap;

    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);

    int baseline = y + (int)(ascent * scale);
    int start_x = x;

    const char *cursor = text;

    while (*cursor != '\0') {

        uint32_t codepoint = text_next_codepoint(&cursor);

        int advance = font_get_advance(&font,
                                      codepoint, scale);

        if (x - start_x + advance > max_width)
            break;

        text_draw_codepoint(&font, codepoint, scale,
                            x, baseline, color,
                            win);

        x += advance;
    }

    return x - start_x;
}

static int text_line_height(const stbtt_fontinfo *font,
                            float scale,
                            int *ascent_out)
{
    int ascent;
    int descent;
    int line_gap;

    stbtt_GetFontVMetrics(font, &ascent,
                           &descent, &line_gap);

    if (ascent_out != NULL)
        *ascent_out = (int)(ascent * scale);

    return (int)((ascent - descent + line_gap) * scale);
}

int draw_text_wrapped(stbtt_fontinfo font, const char *text, int x, int y,
                      int max_width, int max_height, unsigned int color,
                      int font_size, window_t *win)
{
    if (text == NULL || win == NULL ||
        font_size <= 0 || max_width <= 0 ||
        max_height <= 0)
        return 0;

    float scale = stbtt_ScaleForPixelHeight(&font, font_size);

    int ascent_pixels;

    int line_height = text_line_height(
        &font, scale, &ascent_pixels);

    int origin_x = x;
    int baseline = y + ascent_pixels;

    const char *cursor = text;

    while (*cursor != '\0') {

        if (*cursor == '\n') {
            ++cursor;

            x = origin_x;
            y += line_height;
            baseline = y + ascent_pixels;

            if (y + line_height > max_height)
                break;

            continue;
        }

        const char *word_start = cursor;
        int word_width = 0;

        while (*cursor != '\0' && *cursor != '\n') {

            const char *lookahead = cursor;

            uint32_t codepoint =
                text_next_codepoint(&lookahead);

            if (text_is_space(codepoint))
                break;

            word_width += font_get_advance(
                &font, codepoint, scale);

            cursor = lookahead;
        }

        if (x > origin_x &&
            x + word_width > origin_x + max_width) {

            x = origin_x;
            y += line_height;
            baseline = y + ascent_pixels;

            if (y + line_height > max_height)
                break;
        }

        cursor = word_start;

        while (*cursor != '\0' && *cursor != '\n') {

            const char *lookahead = cursor;

            uint32_t codepoint =
                text_next_codepoint(&lookahead);

            if (text_is_space(codepoint))
                break;

            int advance = font_get_advance(
                &font, codepoint, scale);

            text_draw_codepoint(
                &font, codepoint, scale,
                x, baseline, color,
                win);

            x += advance;
            cursor = lookahead;
        }

        while (*cursor != '\0' && *cursor != '\n') {

            const char *lookahead = cursor;

            uint32_t codepoint =
                text_next_codepoint(&lookahead);

            if (!text_is_space(codepoint))
                break;

            x += font_get_advance(
                &font, codepoint, scale);

            cursor = lookahead;
        }
    }

    return 0;
}

int draw_text_word_wrap(stbtt_fontinfo font, const char *text, int x_start, int y_start,
                        int max_width, int max_height, unsigned int color,
                        int font_size, window_t *win)
{
    return draw_text_wrapped(
        font, text,
        x_start, y_start,
        max_width, max_height,
        color, font_size, win);
}


int draw_text_centered(stbtt_fontinfo font, const char *text,
                       int x, int y, int width,
                       unsigned int color, int font_size,
                       window_t *win)
{
    if (!text || !win || font_size <= 0 || width <= 0)
        return 0;

    float scale = stbtt_ScaleForPixelHeight(&font, font_size);

    int text_width = 0;

    const char *cursor = text;

    while (*cursor != '\0') {
        uint32_t codepoint = text_next_codepoint(&cursor);

        text_width += font_get_advance(&font, codepoint, scale);
    }

    int text_x = x + (width - text_width) / 2;

    return draw_text(
        font,
        text,
        text_x,
        y,
        width - (text_x - x),
        color,
        font_size,
        win
    );
}


int draw_text_centered_wrapped(stbtt_fontinfo font, const char *text,
                               int x, int y, int max_width, int max_height,
                               unsigned int color, int font_size,
                               window_t *win)
{
    if (!text || !win || font_size <= 0 ||
        max_width <= 0 || max_height <= 0)
        return 0;

    float scale = stbtt_ScaleForPixelHeight(&font, font_size);

    int ascent;
    int descent;
    int line_gap;

    stbtt_GetFontVMetrics(&font, &ascent, &descent, &line_gap);

    int line_height = (int)((ascent - descent + line_gap) * scale);
    //int ascent_pixels = (int)(ascent * scale);

    const char *cursor = text;
    int line_y = y;
    int max_line_width = 0;

    while (*cursor != '\0') {

        if (line_y + line_height > y + max_height)
            break;

        if (*cursor == '\n') {
            cursor++;
            line_y += line_height;
            continue;
        }

        const char *line_start = cursor;
        const char *last_space = NULL;

        int line_width = 0;
        int width_at_space = 0;

        while (*cursor != '\0' && *cursor != '\n') {

            const char *next = cursor;

            uint32_t cp = text_next_codepoint(&next);

            int advance = font_get_advance(&font, cp, scale);

            if (text_is_space(cp)) {
                last_space = cursor;
                width_at_space = line_width;
            }

            if (line_width + advance > max_width) {

                if (last_space && last_space > line_start) {
                    cursor = last_space;
                    line_width = width_at_space;
                }

                break;
            }

            line_width += advance;
            cursor = next;
        }

        if (cursor == line_start && *cursor != '\0' && *cursor != '\n') {
            const char *next = cursor;
            uint32_t cp = text_next_codepoint(&next);

            line_width = font_get_advance(&font, cp, scale);
            cursor = next;
        }

        int line_x = x + (max_width - line_width) / 2;

        if (line_x < x)
            line_x = x;

        const char *line_end = cursor;

        if (line_end > line_start) {
            const char *draw_end = line_end;

            while (draw_end > line_start &&
                   text_is_space((unsigned char)*(draw_end - 1))) {
                draw_end--;
            }

            size_t length = (size_t)(draw_end - line_start);

            char line_buffer[length + 1];

            memcpy(line_buffer, line_start, length);
            line_buffer[length] = '\0';

            draw_text(
                font,
                line_buffer,
                line_x,
                line_y,
                max_width,
                color,
                font_size,
                win
            );
        }

        if (line_width > max_line_width)
            max_line_width = line_width;

        while (*cursor != '\0' && *cursor != '\n' &&
               text_is_space((unsigned char)*cursor)) {
            cursor++;
        }

        if (*cursor == '\n')
            cursor++;

        line_y += line_height;
    }

    return max_line_width;
}



int text_get_string_width(const stbtt_fontinfo *font, float font_size, const char *text, int cursor_pos)
{
    if (!font || !text || cursor_pos <= 0) {
        return 0;
    }

    float scale = stbtt_ScaleForPixelHeight(font, font_size);
    float width = 0.0f;

    for (int i = 0; i < cursor_pos && text[i] != '\0'; i++) {
        int advance_width = 0;
        int left_side_bearing = 0;

        stbtt_GetCodepointHMetrics(font, (unsigned char)text[i], &advance_width, &left_side_bearing);
        width += (float)advance_width * scale;

        if (i + 1 < cursor_pos && text[i + 1] != '\0') {
            int kern = stbtt_GetCodepointKernAdvance(font, (unsigned char)text[i], (unsigned char)text[i + 1]);
            width += (float)kern * scale;
        }
    }

    return (int)(width + 0.5f);
}
