#ifndef IMAGE_H
#define IMAGE_H

typedef struct image {
    unsigned char* pixels;   // Ponteiro para os dados da imagem (RGBA, 4 canais)
    int width;               // Largura real da imagem
    int height;              // Altura real da imagem
    int channels;            // Quantidade de canais (esperado: 4 para RGBA)
    int pitch;               // Bytes por linha (útil para copiar ou manipular)
} image_t;



int load_image(const char *filename, image_t *img);

void image_free(image_t *img);

unsigned char* draw_image_resize(unsigned char* image_date, int old_width,
    int old_height, int new_width, int new_height);

void draw_image(int dst_x, int dst_y, int screen_width, int screen_height,
    unsigned char* image_buffer, int width, int height,
    unsigned int* framebuffer);

void draw_image_tint(
    int dst_x, int dst_y, int screen_width, int screen_height,
    unsigned char* image_buffer, int width, int height,
    unsigned int* framebuffer, unsigned int color_tint);

void draw_image_tint_shadow(
    int dst_x, int dst_y, int screen_width, int screen_height,
    unsigned char* image_buffer, int width, int height,
    unsigned int* framebuffer, unsigned int color_tint,
    int shadow_offset_x, int shadow_offset_y);


#endif