/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: video.h
 *    Description: Estruturas e declarações globais para o controlo do 
 *                 Framebuffer gráfico e emulação de consola de texto.
 * 
 *         Author: Nelson Cole
 *   Created Date: 27/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 30/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _VIDEO_H_
#define _VIDEO_H_

#include <kernel/boot_info.h>

// contem KERNEL_VIDEO_VIRTUAL_BASE
#include <kernel/kernel/mm/memory_map.h>

// Estrutura que estende a info do bootloader adicionando o estado do cursor
typedef struct {
    unsigned int *frame_buffer_base; // Ponteiro direto para os pixéis (32 bits por cor)
    unsigned long frame_buffer_size;
    
    unsigned int  width;
    unsigned int  height;
    unsigned int  pixels_per_scanLine;

    EFI_GRAPHICS_PIXEL_FORMAT pixel_format;
    
    unsigned int  cursor_x;         // Coluna atual do texto (em pixéis ou caracteres)
    unsigned int  cursor_y;         // Linha atual do texto
    unsigned int  text_color;       // Cor padrão do texto (Ex: 0xFFFFFFFF para Branco)
    unsigned int  background_color; // Cor padrão do fundo (Ex: 0x00000000 para Preto)

    unsigned int  *back_buffer;
} __attribute__((packed)) KERNEL_DISPLAY;

/* ============================================================================
 * VARIÁVEIS GLOBAIS
 * ============================================================================ */

extern KERNEL_DISPLAY g_display;

/* ============================================================================
 * FUNÇÕES VITAIS DE VÍDEO
 * ============================================================================ */

void video_init(BOOT_INFO *boot_info);
void video_flush(void);
void put_pixel(unsigned int x, unsigned int y, unsigned int color);
void video_clear(void);
void video_scroll_up(unsigned int lines);

// Abstração de texto via Framebuffer
void fb_clear(void);
void fb_putc(char c);
void fb_print(const char *str);

#endif // __VIDEO_H__
