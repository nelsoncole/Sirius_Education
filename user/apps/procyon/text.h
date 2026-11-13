#ifndef TEXT_H
#define TEXT_H

#include "window.h"
#include "ported/stb_truetype.h"


typedef struct {
    int width, height, xoff, yoff, advance;
    unsigned char* bitmap; // ponteiro para subimagem no atlas
} glyph_t;

typedef struct {
    glyph_t glyphs[256]; // para ASCII, ou pode ser maior
    unsigned char* atlas_bitmap; // bitmap com todos os glifos
    int atlas_width, atlas_height;
    int font_size;
    // para baseline
    float scale;
    int ascent;
} font_cache_t;

int stbtt_font_init(stbtt_fontinfo *font, const char *path,
                     unsigned char **font_buffer_out);

int draw_text(stbtt_fontinfo font, const char *text,
              int x, int y, int max_width, unsigned int color,
              int font_size, window_t *win);

int draw_text_wrapped(stbtt_fontinfo font, const char *text,
                      int x, int y, int max_width, int max_height,
                      unsigned int color, int font_size, window_t *win);

int draw_text_word_wrap(stbtt_fontinfo font, const char *text,
                        int x_start, int y_start, int max_width, int max_height,
                        unsigned int color, int font_size, window_t *win);

int draw_text_centered(stbtt_fontinfo font, const char *text,
                       int x, int y, int width,
                       unsigned int color, int font_size,
                       window_t *win);
                       
int draw_text_centered_wrapped(stbtt_fontinfo font, const char *text,
                               int x, int y, int max_width, int max_height,
                               unsigned int color, int font_size,
                               window_t *win);


void build_font_cache(stbtt_fontinfo* font, int font_size, font_cache_t *font_cache);
int draw_text_char_cached(font_cache_t *font_cache, unsigned int* buffer, int buf_w, int buf_h, int ch, int x_cursor, int y_cursor, unsigned int color);
void draw_text_cached(font_cache_t *font_cache, unsigned int* buffer, int buf_w, int buf_h,
    const char* text, int x, int y, int max_width, unsigned int color);
void draw_text_cached_rtl(font_cache_t *font_cache, unsigned int* buffer, int buf_w, int buf_h,
                          const char* text, int x, int y, int max_width, unsigned int color);

int text_get_string_width(const stbtt_fontinfo *font, float font_size, const char *text, int cursor_pos);

#endif