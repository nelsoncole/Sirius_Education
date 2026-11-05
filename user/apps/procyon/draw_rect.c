#include "window.h"
#include "draw_rect.h"
#include "pixel.h"

void draw_rect(window_t* win, int x, int y, int w, int h, unsigned int color) {
    if (!win || !win->buffer) return;

    for (int j = 0; j < h; j++) {
        int py = y + j;
        if (py < 0 || py >= win->height) continue;

        for (int i = 0; i < w; i++) {
            int px = x + i;
            if (px < 0 || px >= win->width) continue;

            win->buffer[py * win->width + px] = color;
        }
    }
}

void fill_rect(unsigned int* buffer, int buf_w, int buf_h, int x, int y, int w, int h, unsigned int color) {
    if (!buffer) return;

    for (int j = 0; j < h; j++) {
        int py = y + j;
        if (py < 0 || py >= buf_h) continue;

        for (int i = 0; i < w; i++) {
            int px = x + i;
            if (px < 0 || px >= buf_w) continue;

            buffer[py * buf_w + px] = color;
        }
    }
}

void fill_circle(unsigned int* buffer, int buf_w, int buf_h, int cx, int cy, int r, unsigned int color) {
    if (!buffer) return;

    int r2 = r * r; // raio ao quadrado (evita usar sqrt)
    
    for (int y = -r; y <= r; y++) {
        int py = cy + y;
        if (py < 0 || py >= buf_h) continue;

        for (int x = -r; x <= r; x++) {
            int px = cx + x;
            if (px < 0 || px >= buf_w) continue;

            if (x*x + y*y <= r2) {
                buffer[py * buf_w + px] = color;
            }
        }
    }
}

void draw_rect_alpha(window_t* win, int x, int y, int w, int h, unsigned int color) {
    if (!win || !win->buffer) return;

    unsigned int a = (color >> 24) & 0xFF;
    if (a == 0) return; // totalmente transparente

    float alpha = a / 255.0f;
    unsigned int r = (color >> 16) & 0xFF;
    unsigned int g = (color >> 8)  & 0xFF;
    unsigned int b = (color)       & 0xFF;

    for (int j = 0; j < h; j++) {
        int py = y + j;
        if (py < 0 || py >= win->height) continue;

        for (int i = 0; i < w; i++) {
            int px = x + i;
            if (px < 0 || px >= win->width) continue;

            unsigned int* dst = &win->buffer[py * win->width + px];
            unsigned int dst_color = *dst;

            unsigned int dr = (dst_color >> 16) & 0xFF;
            unsigned int dg = (dst_color >> 8)  & 0xFF;
            unsigned int db = (dst_color)       & 0xFF;

            unsigned int rr = (unsigned int)(r * alpha + dr * (1 - alpha));
            unsigned int rg = (unsigned int)(g * alpha + dg * (1 - alpha));
            unsigned int rb = (unsigned int)(b * alpha + db * (1 - alpha));

            *dst = (0xFF << 24) | (rr << 16) | (rg << 8) | rb;
        }
    }
}


// Desenha apenas a borda de um retângulo no buffer
void draw_rect_border(unsigned int* buffer, int buf_w, int buf_h,
                      int x, int y, int w, int h, unsigned int color) {
    if (!buffer || w <= 0 || h <= 0) return;

    // Linha superior
    for (int i = 0; i < w; i++) {
        int px = x + i;
        int py = y;
        if (px >= 0 && px < buf_w && py >= 0 && py < buf_h)
            buffer[py * buf_w + px] = color;
    }

    // Linha inferior
    for (int i = 0; i < w; i++) {
        int px = x + i;
        int py = y + h - 1;
        if (px >= 0 && px < buf_w && py >= 0 && py < buf_h)
            buffer[py * buf_w + px] = color;
    }

    // Linha esquerda
    for (int j = 0; j < h; j++) {
        int px = x;
        int py = y + j;
        if (px >= 0 && px < buf_w && py >= 0 && py < buf_h)
            buffer[py * buf_w + px] = color;
    }

    // Linha direita
    for (int j = 0; j < h; j++) {
        int px = x + w - 1;
        int py = y + j;
        if (px >= 0 && px < buf_w && py >= 0 && py < buf_h)
            buffer[py * buf_w + px] = color;
    }
}

void draw_circle_quadrant(unsigned int* buffer, int buf_w, int buf_h,
                          int cx, int cy, int r,
                          int quadrant, unsigned int color) {
    // Midpoint Circle Algorithm
    int x = 0;
    int y = r;
    int d = 1 - r;

    while (y >= x) {
        if (quadrant == 0) { // canto superior esquerdo
            draw_pixel(cx - x, cy - y, buf_w, buf_h, color, buffer);
            draw_pixel(cx - y, cy - x, buf_w, buf_h, color, buffer);
        } else if (quadrant == 1) { // canto superior direito
            draw_pixel(cx + x, cy - y, buf_w, buf_h, color, buffer);
            draw_pixel(cx + y, cy - x, buf_w, buf_h, color, buffer);
        } else if (quadrant == 2) { // canto inferior direito
            draw_pixel(cx + x, cy + y, buf_w, buf_h, color, buffer);
            draw_pixel(cx + y, cy + x, buf_w, buf_h, color, buffer);
        } else if (quadrant == 3) { // canto inferior esquerdo
            draw_pixel(cx - x, cy + y, buf_w, buf_h, color, buffer);
            draw_pixel(cx - y, cy + x, buf_w, buf_h, color, buffer);
        }

        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

void draw_rect_border_rounded(unsigned int* buffer, int buf_w, int buf_h,
                              int x, int y, int w, int h, int radius, unsigned int color) {
    // Linhas horizontais (sem as curvas)
    for (int i = x + radius; i < x + w - radius; i++) {
        draw_pixel(i, y, buf_w, buf_h, color, buffer);           // topo
        draw_pixel(i, y + h - 1, buf_w, buf_h, color, buffer);   // fundo
    }

    // Linhas verticais (sem as curvas)
    for (int j = y + radius; j < y + h - radius; j++) {
        draw_pixel(x, j, buf_w, buf_h,color, buffer);           // esquerda
        draw_pixel(x + w - 1, j, buf_w, buf_h, color, buffer);   // direita
    }

    // Canto superior esquerdo
    draw_circle_quadrant(buffer, buf_w, buf_h, x + radius, y + radius, radius, 0, color);
    // Canto superior direito
    draw_circle_quadrant(buffer, buf_w, buf_h, x + w - radius - 1, y + radius, radius, 1, color);
    // Canto inferior direito
    draw_circle_quadrant(buffer, buf_w, buf_h, x + w - radius - 1, y + h - radius - 1, radius, 2, color);
    // Canto inferior esquerdo
    draw_circle_quadrant(buffer, buf_w, buf_h, x + radius, y + h - radius - 1, radius, 3, color);
}

void draw_rect_filled_rounded(unsigned int* buffer, int buf_w, int buf_h,
                              int x, int y, int w, int h, int radius, unsigned int color) {
    // Parte central (sem considerar cantos)
    for (int j = y + radius; j < y + h - radius; j++) {
        for (int i = x; i < x + w; i++) {
            draw_pixel(i, j, buf_w, buf_h, color, buffer);
        }
    }

    // Faixas horizontais (topo e fundo, sem cantos)
    for (int j = 0; j < radius; j++) {
        for (int i = x + radius; i < x + w - radius; i++) {
            draw_pixel(i, y + j, buf_w, buf_h, color, buffer);         // topo
            draw_pixel(i, y + h - j - 1, buf_w, buf_h, color, buffer); // fundo
        }
    }

    // Preencher cantos arredondados
    int cx[4] = { x + radius, x + w - radius - 1, x + w - radius - 1, x + radius };
    int cy[4] = { y + radius, y + radius, y + h - radius - 1, y + h - radius - 1 };

    for (int q = 0; q < 4; q++) {
        for (int yy = -radius; yy <= radius; yy++) {
            for (int xx = -radius; xx <= radius; xx++) {
                if (xx*xx + yy*yy <= radius*radius) {
                    int px = cx[q] + xx;
                    int py = cy[q] + yy;
                    draw_pixel(px, py, buf_w, buf_h, color, buffer);
                }
            }
        }
    }
}


void draw_rect_component(unsigned int* buffer, int buf_w, int buf_h,
                         int x, int y, int w, int h,
                         unsigned int fill_color,
                         unsigned int border_color,
                         int border_thickness,
                         int radius) 
{
    if (w <= 0 || h <= 0) return;

    // 1. Desenha preenchimento
    if (radius > 0) {
        draw_rect_filled_rounded(buffer, buf_w, buf_h,
                                 x, y, w, h, radius, fill_color);
    } else {
        for (int j = y; j < y + h; j++) {
            for (int i = x; i < x + w; i++) {
                draw_pixel(i, j, buf_w, buf_h, fill_color, buffer);
            }
        }
    }

    // 2. Desenha borda
    for (int t = 0; t < border_thickness; t++) {
        if (radius > 0) {
            draw_rect_border_rounded(buffer, buf_w, buf_h,
                                     x + t, y + t, w - 2*t, h - 2*t,
                                     radius, border_color);
        } else {
            draw_rect_border(buffer, buf_w, buf_h,
                             x + t, y + t, w - 2*t, h - 2*t,
                             border_color);
        }
    }
}
