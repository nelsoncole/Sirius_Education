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

#include <kernel/drivers/video/video.h>
#include <kernel/drivers/char/font.h>
#include <kernel/lib/stddef.h>


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

/*
void fb_putc(char c)
{
    // 1. Processamento de caracteres de controlo
    if (c == '\n') {
        g_display.cursor_x = 0;
        g_display.cursor_y += FONT_HEIGHT;
        goto check_screen_end;
    }
    
    if (c == '\r') {
        g_display.cursor_x = 0;
        return;
    }

    if (c == '\t') {
        g_display.cursor_x += (FONT_WIDTH * 4);
        goto check_bounds;
    }

    // Garante que o caractere está nos limites da tabela (0 a 127)
    uint8_t font_index = (uint8_t)c;
    if (font_index >= 128) {
        return;
    }

    // Desenha o caractere na tela
    for (int cy = 0; cy < FONT_HEIGHT; cy++) {
        uint8_t line = g_font_bitmap[font_index][cy];
        
        for (int cx = 0; cx < FONT_WIDTH; cx++) {
            if (line & (0x80 >> cx)) {
                put_pixel(g_display.cursor_x + cx, g_display.cursor_y + cy, g_display.text_color);
            } else {
                put_pixel(g_display.cursor_x + cx, g_display.cursor_y + cy, g_display.background_color);
            }
        }
    }

    // Avança o cursor horizontalmente
    g_display.cursor_x += FONT_WIDTH;

check_bounds:
    // Quebra automática de linha (wrap) se estourar a largura
    if (g_display.cursor_x + FONT_WIDTH > g_display.width) {
        g_display.cursor_x = 0;
        g_display.cursor_y += FONT_HEIGHT;
    }

check_screen_end:
    // Se ultrapassar a altura da tela: limpa o ecrã e reinicia o cursor
    if (g_display.cursor_y + FONT_HEIGHT > g_display.height) {
        // Substitui pela tua função real de limpar tela, ex: fb_clear() ou um loop de pixeis
        //clear_screen(g_display.background_color); 
        fb_clear();
        
        g_display.cursor_x = 0;
        g_display.cursor_y = 0;
    }
}*/

extern void *sse_memcpy(void *s1, const void *s2, size_t len);
extern void *sse_memset_dword(void *dst, uint32_t value, size_t count);
extern void *avx2_memcpy(void *s1, const void *s2, size_t len);
extern volatile int g_cpu_has_avx2;

void fb_putc(char c)
{
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
        g_display.cursor_x += (FONT_WIDTH * 4);
        goto check_bounds;
    }

    uint8_t font_index = (uint8_t)c;
    if (font_index >= 128) {
        return;
    }

    for (int cy = 0; cy < FONT_HEIGHT; cy++) {
        uint8_t line = g_font_bitmap[font_index][cy];
        
        for (int cx = 0; cx < FONT_WIDTH; cx++) {
            if (line & (0x80 >> cx)) {
                put_pixel(g_display.cursor_x + cx, g_display.cursor_y + cy, g_display.text_color);
            } else {
                put_pixel(g_display.cursor_x + cx, g_display.cursor_y + cy, g_display.background_color);
            }
        }
    }

    g_display.cursor_x += FONT_WIDTH;

check_bounds:
    if (g_display.cursor_x + FONT_WIDTH > g_display.width) {
        g_display.cursor_x = 0;
        g_display.cursor_y += FONT_HEIGHT;
    }

check_scroll:
    if (g_display.cursor_y + FONT_HEIGHT > g_display.height) {
        
        // 1. Linha de scan por bytes (A largura do ecrã em bytes: píxeis * 4)
        unsigned int bytes_per_scanline = g_display.pixels_per_scanLine * 4;
        
        // 2. Tamanho de uma linha completa de texto em Bytes
        unsigned int text_line_bytes = FONT_HEIGHT * bytes_per_scanline;
        
        // 3. Tamanho total do ecrã útil a ser deslocado (em Bytes)
        unsigned int total_display_bytes = g_display.height * bytes_per_scanline;
        unsigned int bytes_to_copy = total_display_bytes - text_line_bytes;

        // 4. Definição dos ponteiros base
        unsigned int *dst = g_display.frame_buffer_base;
        
        // Deslocamento de píxeis na aritmética do ponteiro unsigned int (píxeis = bytes / 4)
        unsigned int *src = g_display.frame_buffer_base + (text_line_bytes / 4);

        // Copia todas as linhas para cima via SSE usando bytes exatos

        if(g_cpu_has_avx2) avx2_memcpy(dst, src, bytes_to_copy);
        else sse_memcpy(dst, src, bytes_to_copy);

        // 5. Limpar a última linha que ficou duplicada no fundo
        unsigned int *last_line_start = g_display.frame_buffer_base + (bytes_to_copy / 4);

        // Preenche com a cor de fundo usando a tua sse_memset_dword (otimizada para 32-bits/4 bytes)
        // O tamanho passado para sse_memset_dword deve ser a contagem de DWORDS (píxeis), não bytes!
        unsigned int text_line_pixels = FONT_HEIGHT * g_display.pixels_per_scanLine;
        sse_memset_dword(last_line_start, g_display.background_color, text_line_pixels);
       
        // 6. Reposiciona o cursor de forma segura no início da última linha útil
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


/*
 * Emite um único caractere para o terminal ativo do kernel.
 * Atua como uma camada de abstração sobre o driver de Framebuffer (fb).
 * Esta função é chamada no kprintf.c
 */

void kernel_putchar(char c)
{
    fb_putc(c);
}