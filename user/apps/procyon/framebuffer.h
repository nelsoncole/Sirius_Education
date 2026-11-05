#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

void framebuffer_clear(unsigned int color);
void framebuffer_put_pixel(int x, int y, unsigned int color);
void framebuffer_blend_pixel(int x, int y, unsigned int color, float alpha);

#endif