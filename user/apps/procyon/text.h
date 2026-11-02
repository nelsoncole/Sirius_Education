#ifndef TEXT_H
#define TEXT_H

#include "window.h"
#include "ported/stb_truetype.h"

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

#endif