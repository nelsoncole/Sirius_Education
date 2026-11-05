#include "framebuffer.h"
#include <string.h>

extern unsigned int* back_buffer;
extern int screen_width;
extern int screen_height;
extern int pitch;

void framebuffer_clear(unsigned int color) {
    memset(back_buffer, color, pitch * screen_height * 4);
}


void framebuffer_put_pixel(int x, int y, unsigned int color) {
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height) return;
    back_buffer[y * (pitch /4) + x] = color;
}

void framebuffer_blend_pixel(int x, int y, unsigned int color, float alpha) {
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height) return;

    unsigned int* pixel = &back_buffer[y * (pitch /4) + x];
    unsigned int dst = *pixel;

    unsigned char r1 = (dst >> 16) & 0xFF;
    unsigned char g1 = (dst >> 8) & 0xFF;
    unsigned char b1 = dst & 0xFF;

    unsigned char r2 = (color >> 16) & 0xFF;
    unsigned char g2 = (color >> 8) & 0xFF;
    unsigned char b2 = color & 0xFF;

    unsigned char r = (unsigned char)((1 - alpha) * r1 + alpha * r2);
    unsigned char g = (unsigned char)((1 - alpha) * g1 + alpha * g2);
    unsigned char b = (unsigned char)((1 - alpha) * b1 + alpha * b2);

    *pixel = (r << 16) | (g << 8) | b;
}
