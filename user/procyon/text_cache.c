
#include "text.h"
#include "pixel.h"
#include "utf8_decode.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define CACHE_START_CP 0x20
#define CACHE_END_CP  0xFF
#define CACHE_COLUMNS 16
#define CACHE_GLYPH_COUNT (CACHE_END_CP - CACHE_START_CP + 1)

void build_font_cache(stbtt_fontinfo *font, int font_size,
                      font_cache_t *font_cache)
{
    if (!font || !font_cache || font_size <= 0)
        return;

    memset(font_cache, 0, sizeof(*font_cache));

    float scale = stbtt_ScaleForPixelHeight(font, font_size);

    int ascent;
    stbtt_GetFontVMetrics(font, &ascent, NULL, NULL);

    font_cache->font_size = font_size;
    font_cache->scale = scale;
    font_cache->ascent = ascent;

    int max_w = 0;
    int max_h = 0;

    for (int cp = CACHE_START_CP; cp <= CACHE_END_CP; cp++) {
        int w, h, xoff, yoff;

        unsigned char *bmp = stbtt_GetCodepointBitmap(
            font, 0, scale, cp,
            &w, &h, &xoff, &yoff);

        if (w > max_w)
            max_w = w;

        if (h > max_h)
            max_h = h;

        if (bmp)
            stbtt_FreeBitmap(bmp, NULL);
    }

    if (max_w <= 0 || max_h <= 0)
        return;

    int rows = (CACHE_GLYPH_COUNT + CACHE_COLUMNS - 1)
               / CACHE_COLUMNS;

    font_cache->atlas_width = max_w * CACHE_COLUMNS;
    font_cache->atlas_height = max_h * rows;

    size_t atlas_size =
        (size_t)font_cache->atlas_width *
        font_cache->atlas_height;

    font_cache->atlas_bitmap = calloc(atlas_size, 1);

    if (!font_cache->atlas_bitmap)
        return;

    for (int cp = CACHE_START_CP; cp <= CACHE_END_CP; cp++) {
        int index = cp - CACHE_START_CP;

        int col = index % CACHE_COLUMNS;
        int row = index / CACHE_COLUMNS;

        int x_pos = col * max_w;
        int y_pos = row * max_h;

        int w, h, xoff, yoff;
        int advance_width;

        stbtt_GetCodepointHMetrics(
            font, cp, &advance_width, NULL);

        unsigned char *bmp = stbtt_GetCodepointBitmap(
            font, 0, scale, cp,
            &w, &h, &xoff, &yoff);

        glyph_t *g = &font_cache->glyphs[cp];

        g->width = w;
        g->height = h;
        g->xoff = xoff;
        g->yoff = yoff;
        g->advance = (int)(advance_width * scale);

        g->bitmap = font_cache->atlas_bitmap
                  + (size_t)y_pos * font_cache->atlas_width
                  + x_pos;

        if (bmp && w > 0 && h > 0) {
            for (int y = 0; y < h; y++) {
                memcpy(
                    g->bitmap + (size_t)y * font_cache->atlas_width,
                    bmp + (size_t)y * w,
                    w
                );
            }
        }

        if (bmp)
            stbtt_FreeBitmap(bmp, NULL);
    }
}

int draw_text_char_cached(font_cache_t *font_cache,
                          unsigned int *buffer,
                          int buf_w, int buf_h,
                          int ch,
                          int x_cursor, int y_cursor,
                          unsigned int color)
{
    if (!font_cache || !buffer ||
        ch < CACHE_START_CP || ch > CACHE_END_CP)
        return 0;

    glyph_t *g = &font_cache->glyphs[ch];

    if (!g->bitmap)
        return 0;

    int baseline = y_cursor
                 + (int)(font_cache->ascent *
                         font_cache->scale);

    for (int y = 0; y < g->height; y++) {
        int py = baseline + y + g->yoff;

        if (py < 0 || py >= buf_h)
            continue;

        for (int x = 0; x < g->width; x++) {
            int px = x_cursor + x + g->xoff;

            if (px < 0 || px >= buf_w)
                continue;

            unsigned char coverage =
                g->bitmap[(size_t)y *
                          font_cache->atlas_width + x];

            if (!coverage)
                continue;

            unsigned int bg = get_pixel(
                px, py, buf_w, buf_h, buffer);

            unsigned int blended =
                blend_color(bg, color, coverage);

            draw_pixel(
                px, py, buf_w, buf_h,
                blended, buffer);
        }
    }

    return g->advance;
}

void draw_text_cached(font_cache_t *font_cache,
                      unsigned int *buffer,
                      int buf_w, int buf_h,
                      const char *text,
                      int x, int y,
                      int max_width,
                      unsigned int color)
{
    if (!font_cache || !text || !buffer ||
        max_width <= 0)
        return;

    int x_cursor = x;
    int limit_x = x + max_width;

    const char *cursor = text;

    while (*cursor) {
        uint32_t cp;
        int consumed = utf8_decode(cursor, &cp);

        if (consumed <= 0) {
            cp = '?';
            consumed = 1;
        }

        cursor += consumed;

        if (cp < CACHE_START_CP || cp > CACHE_END_CP)
            continue;

        glyph_t *g = &font_cache->glyphs[cp];

        if (!g->bitmap)
            continue;

        if (x_cursor + g->advance > limit_x)
            break;

        int advance = draw_text_char_cached(
            font_cache, buffer,
            buf_w, buf_h,
            cp, x_cursor, y, color);

        x_cursor += advance;
    }
}

void draw_text_cached_rtl(font_cache_t *font_cache,
                          unsigned int *buffer,
                          int buf_w, int buf_h,
                          const char *text,
                          int x, int y,
                          int max_width,
                          unsigned int color)
{
    if (!font_cache || !text || !buffer ||
        max_width <= 0)
        return;

    int text_width = 0;
    const char *cursor = text;

    while (*cursor) {
        uint32_t cp;
        int consumed = utf8_decode(cursor, &cp);

        if (consumed <= 0) {
            cp = '?';
            consumed = 1;
        }

        cursor += consumed;

        if (cp < CACHE_START_CP || cp > CACHE_END_CP)
            continue;

        glyph_t *g = &font_cache->glyphs[cp];

        if (!g->bitmap)
            continue;

        if (text_width + g->advance > max_width)
            break;

        text_width += g->advance;
    }

    int x_cursor = x + max_width - text_width;

    draw_text_cached(
        font_cache, buffer,
        buf_w, buf_h,
        text, x_cursor, y,
        text_width, color
    );
}