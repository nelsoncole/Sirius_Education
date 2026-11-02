#include "pixel.h"

unsigned int blend_color(unsigned int bg, unsigned int fg, unsigned char alpha) {

    unsigned int rb = (((fg & 0x00FF00FF) * alpha) + ((bg & 0x00FF00FF) * (255 - alpha))) / 255;

    unsigned int g = (((fg & 0x0000FF00) * alpha) + ((bg & 0x0000FF00) * (255 - alpha))) / 255;

    return 0xFF000000 | (rb & 0x00FF00FF) | (g & 0x0000FF00);
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
