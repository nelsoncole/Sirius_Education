#include "pixel.h"

unsigned int blend_color(unsigned int bg, unsigned int fg, unsigned char alpha)
{
    unsigned int bg_a = (bg >> 24) & 0xFF;

    unsigned int bg_r = (bg >> 16) & 0xFF;
    unsigned int bg_g = (bg >> 8) & 0xFF;
    unsigned int bg_b = bg & 0xFF;

    unsigned int fg_r = (fg >> 16) & 0xFF;
    unsigned int fg_g = (fg >> 8) & 0xFF;
    unsigned int fg_b = fg & 0xFF;

    unsigned int out_a = alpha + (bg_a * (255 - alpha) + 127) / 255;

    if (out_a == 0)
        return 0;

    unsigned int r = (fg_r * alpha + bg_r * bg_a * (255 - alpha) / 255) / out_a;
    unsigned int g = (fg_g * alpha + bg_g * bg_a * (255 - alpha) / 255) / out_a;
    unsigned int b = (fg_b * alpha + bg_b * bg_a * (255 - alpha) / 255) / out_a;

    return (out_a << 24) | (r << 16) | (g << 8) | b;
}

unsigned int alpha_blend(unsigned int src, unsigned int dst)
{
    unsigned int a = src >> 24;

    if (a == 0)
        return dst;

    if (a == 255)
        return src;

    unsigned int ia = 255 - a;

    unsigned int r = (((src >> 16) & 0xFF) * a +
                  ((dst >> 16) & 0xFF) * ia) / 255;

    unsigned int g = (((src >> 8) & 0xFF) * a +
                  ((dst >> 8) & 0xFF) * ia) / 255;

    unsigned int b = ((src & 0xFF) * a +
                  (dst & 0xFF) * ia) / 255;

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}


void draw_pixel(int x, int y, int width, int height, unsigned int color, unsigned int *buffer) {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return; // pixel fora da tela

    buffer[y * width + x] = color;
}

unsigned int get_pixel(int x, int y, int width, int height, unsigned int *buffer) {
    if (x < 0 || x >= width || y < 0 || y >= height)
        return 0; // pixel fora da tela (preto ou fundo padrão)

    return buffer[y * width + x];
}
