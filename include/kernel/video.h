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
 *  Modified Date: 27/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef __VIDEO_H__
#define __VIDEO_H__

#include <kernel/boot_info.h>

#define KERNEL_VIDEO_VIRTUAL_BASE  0xFFFF8000E0000000UL

// Estrutura que estende a info do bootloader adicionando o estado do cursor
typedef struct {
    unsigned int *frame_buffer_base; // Ponteiro direto para os pixéis (32 bits por cor)
    unsigned long frame_buffer_size;
    
    unsigned int  width;
    unsigned int  height;
    unsigned int  pixels_per_scanLine;
    
    unsigned int  cursor_x;         // Coluna atual do texto (em pixéis ou caracteres)
    unsigned int  cursor_y;         // Linha atual do texto
    unsigned int  text_color;       // Cor padrão do texto (Ex: 0xFFFFFFFF para Branco)
    unsigned int  background_color; // Cor padrão do fundo (Ex: 0x00000000 para Preto)
} __attribute__((packed)) KERNEL_DISPLAY;

/* ============================================================================
 * VARIÁVEIS GLOBAIS
 * ============================================================================ */

extern KERNEL_DISPLAY g_display;

/* ============================================================================
 * FUNÇÕES VITAIS DE VÍDEO
 * ============================================================================ */

void video_init(GRAPHIC_INFO *graphic_info);
void put_pixel(unsigned int x, unsigned int y, unsigned int color);

#endif // __VIDEO_H__
