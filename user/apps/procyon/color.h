#ifndef COLOR_H
#define COLOR_H

typedef unsigned int rgb_t;

#define RGBA_TO_ARGB(rgba) ( \
    (((rgba) >> 0)  & 0xFF) << 24 | /* Alpha  */ \
    (((rgba) >> 24) & 0xFF) << 16 | /* Red    */ \
    (((rgba) >> 16) & 0xFF) << 8  | /* Green  */ \
    (((rgba) >> 8)  & 0xFF)        /* Blue   */ \
)

unsigned int rgb(unsigned char r, unsigned char g, unsigned char b);
unsigned int rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

#endif