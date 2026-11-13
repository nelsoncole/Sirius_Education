#define STB_IMAGE_IMPLEMENTATION
#include "ported/stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "ported/stb_image_resize.h"


#include "image.h"

#include <stdio.h>
/*
    Formato suportados:
        JPEG, PNG, BMP, TGA, PSD, GIF, HDR, PIC, PNM
    Formatos não suportados
        WebP, TIFF, HEIF / AVIF, JPEG 2000, SVG, GIF animado
*/
int load_image(const char *filename, image_t *img) {
    int img_width = 0, img_height =0, img_channels = 0;
    
    // Força RGBA (4 canais)
    unsigned char* data = stbi_load(filename, &img_width, &img_height, &img_channels, 4); 
    
    if(!data) {
        printf("Erro ao carregar imagem: %s\n", filename);
        img->pixels = NULL;
        img->width = 0;
        img->height = 0;
        img->channels = 0;
        return 1; 
    }

    img->pixels = data;
    img->width = img_width; 
    img->height = img_height;
    img->channels = img_channels;

    return 0;
}

void image_free(image_t *img) {
    if(!img) return;

    stbi_image_free(img->pixels);
}

unsigned char* draw_image_resize(unsigned char* image_date, int old_width, int old_height, int new_width, int new_height) {
    unsigned char *resized = malloc(new_width * new_height * 4); // 4 canais (RGBA)

    stbir_resize_uint8(image_date, old_width, old_height, 0,
                   resized, new_width, new_height, 0, 4);

    return resized;
}

// Função que desenha uma imagem PNG no framebuffer
void draw_image(int dst_x, int dst_y, int screen_width,
    int screen_height, unsigned char* image_buffer,
    int width, int height, unsigned int* framebuffer) {

    if(!image_buffer || !framebuffer) return;

    for (int y = 0; y < height && (y + dst_y) < screen_height; y++) {
        for (int x = 0; x < width && (x + dst_x) < screen_width; x++) {
            int idx = (y * width + x) * 4;

            unsigned char r = image_buffer[idx + 0];
            unsigned char g = image_buffer[idx + 1];
            unsigned char b = image_buffer[idx + 2];
            unsigned char a = image_buffer[idx + 3];

            // Convertendo RGBA para ARGB32 (0xAARRGGBB)
            unsigned int color = (a << 24) | (r << 16) | (g << 8) | b;

            // Mistura alfa simples com fundo (se quiser fazer blending)
            int screen_idx = (y + dst_y) * screen_width + (x + dst_x);
            framebuffer[screen_idx] = color;
        }
    }
}

// color_tint no formato 0xRRGGBB (sem alpha)
void draw_image_tint(int dst_x, int dst_y, int screen_width, int screen_height,
    unsigned char* image_buffer, int width, int height,
    unsigned int* framebuffer, unsigned int color_tint) {

    if (!image_buffer || !framebuffer) return;

    unsigned char tint_r = (color_tint >> 16) & 0xFF;
    unsigned char tint_g = (color_tint >> 8) & 0xFF;
    unsigned char tint_b = color_tint & 0xFF;

    for (int y = 0; y < height && (y + dst_y) < screen_height; y++) {
        for (int x = 0; x < width && (x + dst_x) < screen_width; x++) {
            int idx = (y * width + x) * 4;

            unsigned char src_r = image_buffer[idx + 0];
            unsigned char src_g = image_buffer[idx + 1];
            unsigned char src_b = image_buffer[idx + 2];
            unsigned char src_a = image_buffer[idx + 3];

            if (src_a == 0) continue; // Totalmente transparente

            // Aplicar a tint
            unsigned char r = (src_r * tint_r) / 255;
            unsigned char g = (src_g * tint_g) / 255;
            unsigned char b = (src_b * tint_b) / 255;

            int screen_idx = (y + dst_y) * screen_width + (x + dst_x);
            unsigned int dst = framebuffer[screen_idx];

            unsigned char dst_r = (dst >> 16) & 0xFF;
            unsigned char dst_g = (dst >> 8) & 0xFF;
            unsigned char dst_b = dst & 0xFF;

            float alpha = src_a / 255.0f;

            // Blending simples: C_result = C_src * alpha + C_dst * (1 - alpha)
            unsigned char out_r = (unsigned char)(r * alpha + dst_r * (1.0f - alpha));
            unsigned char out_g = (unsigned char)(g * alpha + dst_g * (1.0f - alpha));
            unsigned char out_b = (unsigned char)(b * alpha + dst_b * (1.0f - alpha));

            // Usa alpha original do ícone como canal alfa final (opcional)
            unsigned int out_color = (src_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
            framebuffer[screen_idx] = out_color;
        }
    }
}


void draw_image_tint_shadow(
    int dst_x, int dst_y, int screen_width, int screen_height,
    unsigned char* image_buffer, int width, int height,
    unsigned int* framebuffer, unsigned int color_tint,
    int shadow_offset_x, int shadow_offset_y) {

    // Cor da sombra: preta com alfa reduzido (ex: 80 de opacidade)
    //unsigned int shadow_color = 0x000000;
    unsigned char shadow_alpha = 80;

    // Primeiro desenha a sombra
    for (int y = 0; y < height && (y + dst_y + shadow_offset_y) < screen_height; y++) {
        for (int x = 0; x < width && (x + dst_x + shadow_offset_x) < screen_width; x++) {
            int idx = (y * width + x) * 4;

            unsigned char src_a = image_buffer[idx + 3];
            if (src_a == 0) continue;

            float alpha = (src_a / 255.0f) * (shadow_alpha / 255.0f);

            int screen_idx = (y + dst_y + shadow_offset_y) * screen_width + (x + dst_x + shadow_offset_x);
            unsigned int dst = framebuffer[screen_idx];

            unsigned char dst_r = (dst >> 16) & 0xFF;
            unsigned char dst_g = (dst >> 8) & 0xFF;
            unsigned char dst_b = dst & 0xFF;

            unsigned char out_r = (unsigned char)(0 * alpha + dst_r * (1.0f - alpha));
            unsigned char out_g = (unsigned char)(0 * alpha + dst_g * (1.0f - alpha));
            unsigned char out_b = (unsigned char)(0 * alpha + dst_b * (1.0f - alpha));

            unsigned int out_color = (255 << 24) | (out_r << 16) | (out_g << 8) | out_b;
            framebuffer[screen_idx] = out_color;
        }
    }

    // Depois desenha o ícone com a cor desejada por cima
    draw_image_tint(dst_x, dst_y, screen_width, screen_height,
                    image_buffer, width, height, framebuffer, color_tint);
}
