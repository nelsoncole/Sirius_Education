#include "color.h"

unsigned int rgb(unsigned char r, unsigned char g, unsigned char b) {
    return (r << 16) | (g << 8) | b;
}

unsigned int rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    return (r << 24) | (g << 16) | (b << 8) | a;
}
