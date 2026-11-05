#ifndef DRAW_RECT_H
#define DRAW_RECT_H

#include "window.h"

void draw_rect(window_t* win, int x, int y, int w, int h, unsigned int color);
void draw_rect_alpha(window_t* win, int x, int y, int w, int h, unsigned int color);

void fill_rect(unsigned int* buffer, int buf_w, int buf_h,
                int x, int y, int w, int h, unsigned int color);
                
void fill_circle(unsigned int* buffer, int buf_w, int buf_h,
                int cx, int cy, int r, unsigned int color);

void draw_rect_border(unsigned int* buffer, int buf_w, int buf_h,
                      int x, int y, int w, int h, unsigned int color);

void draw_circle_quadrant(unsigned int* buffer, int buf_w, int buf_h,
                          int cx, int cy, int r,
                          int quadrant, unsigned int color);                      

void draw_rect_border_rounded(unsigned int* buffer, int buf_w, int buf_h,
                              int x, int y, int w, int h, int radius, unsigned int color);

void draw_rect_filled_rounded(unsigned int* buffer, int buf_w, int buf_h,
                              int x, int y, int w, int h, int radius, unsigned int color);

void draw_rect_component(unsigned int* buffer, int buf_w, int buf_h,
                         int x, int y, int w, int h,
                         unsigned int fill_color,
                         unsigned int border_color,
                         int border_thickness,
                         int radius);


#endif