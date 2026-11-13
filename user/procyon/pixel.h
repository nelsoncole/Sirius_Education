#ifndef PIXEL_H
#define PIXEL_H

unsigned int blend_color(unsigned int bg, unsigned int fg, unsigned char alpha);
unsigned int alpha_blend(unsigned int src, unsigned int dst);
void draw_pixel(int x, int y, int width, int height, unsigned int color, unsigned int *buffer);
unsigned int get_pixel(int x, int y, int width, int height, unsigned int *buffer);

#endif
