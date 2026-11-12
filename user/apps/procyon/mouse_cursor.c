#include "mouse_cursor.h"
#include <stdlib.h>

// Definição exata das cores com suporte a Anti-Aliasing (Canal Alpha)
#define CURSOR_BLACK       0xFF000000  // Borda preta opaca
#define CURSOR_WHITE       0xFFFFFFFF  // Preenchimento branco opaco
#define CURSOR_DARK_GRAY   0x8A000000  // Suavização externa escura (Alpha ~54%)
#define CURSOR_SHADOW      0x30000000  // Sombra suave integrada (Alpha ~18%)
#define CURSOR_TRANSPARENT 0x00000000  // Transparência total

mouse_cursor_t g_mouse_cursor;

static void cursor_set_pixel(unsigned int* buffer, int x, int y, unsigned int color)
{
    if (x < 0 || y < 0 || x >= CURSOR_W || y >= CURSOR_H)
        return;

    buffer[y * CURSOR_W + x] = color;
}

static void cursor_draw_arrow(unsigned int* buffer)
{
    // 'B' = Preto, 'W' = Branco, 'G' = Suavização, ' ' = Transparente
    static const char* arrow[] =
    {
        "B                               ", // 0
        "BB                              ", // 1
        "BWB                             ", // 2
        "BWWB                            ", // 3
        "BWWWB                           ", // 4
        "BWWWWB                          ", // 5
        "BWWWWWB                         ", // 6
        "BWWWWWWB                        ", // 7
        "BWWWWWWWB                       ", // 8
        "BWWWWWWWWB                      ", // 9
        "BWWWWWWWWWB                     ", // 10
        "BWWWWWWWWWWB                    ", // 11
        "BWWWWWWWWWWWB                   ", // 12
        "BWWWWWWWWWWWWB                  ", // 13
        "BWWWWBBBBBBBBBB                 ", // 14 - Base da cauda fixa e limpa
        "BWWWB  BB                       ", // 15 - Início do preenchimento da cauda
        "BWWB    BWB                     ", // 16 - Transição suave e reta
        "BWB     BWWB                    ", // 17
        "BB       BWB                    ", // 18
        "G        BWWB                   ", // 19
        "          BWB                   ", // 20
        "          BWWB                  ", // 21
        "           BWB                  ", // 22
        "           BB                   "  // 23 - Fim exato da cauda
    };

    int num_rows = sizeof(arrow) / sizeof(arrow[0]);

    for (int y = 0; y < num_rows; y++)
    {
        for (int x = 0; arrow[y][x] != '\0' && x < CURSOR_W; x++)
        {
            switch (arrow[y][x])
            {
                case 'B':
                    cursor_set_pixel(buffer, x, y, CURSOR_BLACK);
                    break;
                case 'W':
                    cursor_set_pixel(buffer, x, y, CURSOR_WHITE);
                    break;
                case 'G':
                    cursor_set_pixel(buffer, x, y, CURSOR_DARK_GRAY);
                    break;
                default:
                    // Deixa transparente
                    break;
            }
        }
    }
}

void mouse_cursor_init(void)
{
    // Liberta se já existir para evitar fugas de memória
    if (g_mouse_cursor.buffer)
    {
        free(g_mouse_cursor.buffer);
        g_mouse_cursor.buffer = NULL;
    }

    // Configura os parâmetros corretos para a nova proporção 32x32
    g_mouse_cursor.width = CURSOR_W;
    g_mouse_cursor.height = CURSOR_H;
    g_mouse_cursor.visible = 1;

    // Aloca o buffer (32x32 * 4 bytes)
    g_mouse_cursor.buffer = (unsigned int*)malloc(CURSOR_W * CURSOR_H * sizeof(unsigned int));
    if (!g_mouse_cursor.buffer)
        return;

    // Limpa o fundo com transparência absoluta
    for (int i = 0; i < CURSOR_W * CURSOR_H; i++)
    {
        g_mouse_cursor.buffer[i] = CURSOR_TRANSPARENT;
    }

    // Renderiza a nova matriz do Windows Aero no buffer
    cursor_draw_arrow(g_mouse_cursor.buffer);
}

void set_cursor(int x, int y)
{
    g_mouse_cursor.x = x;
    g_mouse_cursor.y = y;
}