/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: char.c
 *    Description: Gestão e renderização de caracteres no ecrã (Console Core).
 *                 Processa caracteres ASCII imprimíveis e de controlo,
 *                 interagindo com a tabela bitmap font e o framebuffer global.
 * 
 *         Author: Nelson Cole
 *   Created Date: 28/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 28/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/drivers/video.h>
#include <kernel/drivers/font.h>


/*
 *
 * Limpa completamente o ecrã (Preenche com a cor de fundo atual) e reseta 
 * as coordenadas do cursor de escrita de volta para a origem (0,0).
 * 
 */
void fb_clear(void)
{
    // 1. Calcular o total de pixels contidos no Framebuffer de vídeo
    unsigned int total_pixels = g_display.height * g_display.pixels_per_scanLine;

    // 2. Preencher toda a memória de vídeo com a cor de fundo definida
    unsigned int *fb = g_display.frame_buffer_base;
    for (unsigned int i = 0; i < total_pixels; i++) {
        fb[i] = g_display.background_color;
    }

    // 3. Resetar as coordenadas do cursor para o canto superior esquerdo
    g_display.cursor_x = 0;
    g_display.cursor_y = 0;
}


void fb_putc(char c)
{
    // 1. Processamento de caracteres de controlo (Escape Sequences)
    if (c == '\n') {
        g_display.cursor_x = 0;
        g_display.cursor_y += FONT_HEIGHT;
        goto check_scroll;
    }
    
    if (c == '\r') {
        g_display.cursor_x = 0;
        return;
    }

    if (c == '\t') {
        g_display.cursor_x += (FONT_WIDTH * 4); // Avança 4 espaços
        goto check_bounds;
    }

    // Garante que o caractere está nos limites da tabela de 128 glifos (0 a 127)
    // Usamos uint8_t no cast para evitar problemas com valores negativos
    uint8_t font_index = (uint8_t)c;
    if (font_index >= 128) {
        return;
    }

    // Desenha as 16 linhas do caractere usando a matriz de bytes
    for (int cy = 0; cy < FONT_HEIGHT; cy++) {
        uint8_t line = g_font_bitmap[font_index][cy];
        
        // Desenha os 8 bits (pixels) da linha atual da esquerda para a direita
        for (int cx = 0; cx < FONT_WIDTH; cx++) {
            // Verifica o bit mais significativo (MSB) deslocando o bit correspondente
            if (line & (0x80 >> cx)) {
                put_pixel(g_display.cursor_x + cx, g_display.cursor_y + cy, g_display.text_color);
            } else {
                put_pixel(g_display.cursor_x + cx, g_display.cursor_y + cy, g_display.background_color);
            }
        }
    }

    // Avança o cursor horizontalmente para o próximo caractere
    g_display.cursor_x += FONT_WIDTH;

check_bounds:
    // Se o texto estourar a largura do ecrã, faz quebra automática de linha (wrap)
    if (g_display.cursor_x + FONT_WIDTH > g_display.width) {
        g_display.cursor_x = 0;
        g_display.cursor_y += FONT_HEIGHT;
    }

check_scroll:
    /*
     * ========================================================
     * CONTROLO DE SCROLL REAL DO FRAMEBUFFER
     *
     * Se o cursor ultrapassar a altura útil do ecrã, movemos
     * todas as linhas de pixels para cima (na vertical) à 
     * distância exata de uma linha de texto (FONT_HEIGHT). 
     * A última linha é limpa com a cor de fundo.
     * ========================================================
     */
    if (g_display.cursor_y + FONT_HEIGHT > g_display.height) {
        
        // 1. Calcular o número de pixels numa linha completa de texto (8x16)
        unsigned int scanline_words = g_display.pixels_per_scanLine;
        unsigned int text_line_pixels = FONT_HEIGHT * scanline_words;
        
        // 2. Calcular o total de pixels do ecrã inteiro menos a primeira linha de texto
        unsigned int total_display_pixels = g_display.height * scanline_words;
        unsigned int pixels_to_copy = total_display_pixels - text_line_pixels;

        // 3. Copiar os pixels para cima (Deslocar a imagem)
        // Movemos a partir da segunda linha de texto para o início do Framebuffer
        unsigned int *dst = g_display.frame_buffer_base;
        unsigned int *src = g_display.frame_buffer_base + text_line_pixels;

        for (unsigned int i = 0; i < pixels_to_copy; i++) {
            dst[i] = src[i];
        }

        // 4. Limpar a última linha que ficou duplicada no fundo (Preencher com Background Color)
        unsigned int *last_line_start = g_display.frame_buffer_base + pixels_to_copy;
        for (unsigned int i = 0; i < text_line_pixels; i++) {
            last_line_start[i] = g_display.background_color;
        }

        // 5. Ajustar o cursor para o início da última linha do ecrã
        g_display.cursor_x = 0;
        g_display.cursor_y = g_display.height - FONT_HEIGHT;
    }

}

/*
 * 
 * Imprime uma string (cadeia de caracteres) terminada em '\0' utilizando
 * a infraestrutura do dispositivo gráfico de Framebuffer.
 * 
 */
void fb_print(const char *str)
{
    while (*str) {
        fb_putc(*str);
        str++; // Avança para o próximo caractere na string
    }
}
