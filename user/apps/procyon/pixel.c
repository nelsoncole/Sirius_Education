#include "pixel.h"

unsigned int blend_color(unsigned int bg, unsigned int fg, unsigned char alpha) {
    unsigned char bg_r = (bg >> 16) & 0xFF;
    unsigned char bg_g = (bg >> 8) & 0xFF;
    unsigned char bg_b = bg & 0xFF;

    unsigned char fg_r = (fg >> 16) & 0xFF;
    unsigned char fg_g = (fg >> 8) & 0xFF;
    unsigned char fg_b = fg & 0xFF;

    unsigned char r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
    unsigned char g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
    unsigned char b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;

    return (r << 16) | (g << 8) | b;
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
