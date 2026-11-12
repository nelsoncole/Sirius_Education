#include "draw_line.h"
#include "pixel.h"
#include <stdlib.h>

// Algoritmo de Bresenham
void draw_line(unsigned int* buffer, int win_w, int win_h,
               int x0, int y0, int x1, int y1, unsigned int color) 
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        draw_pixel(x0, y0, win_w, win_h, color, buffer);

        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}