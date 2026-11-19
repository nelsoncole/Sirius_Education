#include "color.h"

unsigned int rgb(unsigned char r, unsigned char g, unsigned char b) {
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

unsigned int argb(unsigned char a, unsigned char r, unsigned char g, unsigned char b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

unsigned int rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return (r << 24) | (g << 16) | (b << 8) | a;
}
