/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: video.c
 *    Description: Inicialização e manipulação do dispositivo de exibição global.
 * 
 *         Author: Nelson Cole
 *   Created Date: 28/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 29/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/drivers/video.h>

KERNEL_DISPLAY g_display;

void video_init(BOOT_INFO *boot_info) 
{
    GRAPHIC_INFO *graphic_info = (GRAPHIC_INFO *)&boot_info->Graphics;

    g_display.frame_buffer_base     = (unsigned int *) KERNEL_VIDEO_VIRTUAL_BASE;//graphic_info->FrameBufferBase;
    g_display.frame_buffer_size     = graphic_info->FrameBufferSize;
    g_display.width                 = graphic_info->Width;
    g_display.height                = graphic_info->Height;
    g_display.pixels_per_scanLine   = graphic_info->PixelsPerScanLine;
    g_display.pixel_format          = graphic_info->PixelFormat;
    g_display.cursor_x              = 0;
    g_display.cursor_y              = 0;
    g_display.text_color            = 0xFFFFFFFF;       // Cor padrão do texto (Ex: 0xFFFFFFFF para Branco)
    g_display.background_color      = 0x00000000; // Cor padrão do fundo (Ex: 0x00000000 para Preto)

    fb_clear();
}

// Função base para o teu futuro gestor de janelas desenhar retângulos, bordas, etc.
void put_pixel(unsigned int x, unsigned int y, unsigned int color)
{
    // Proteção básica contra escrita fora dos limites físicos do ecrã
    if (x >= g_display.width || y >= g_display.height) {
        return;
    }
    
    // Calcula o offset linear exato na memória de vídeo
    g_display.frame_buffer_base[y * g_display.pixels_per_scanLine + x] = color;
}
