#ifndef MOUSE_CURSOR_H
#define MOUSE_CURSOR_H

// Tamanho padrão do Windows para cursores em baixa/média resolução
#define CURSOR_W 32
#define CURSOR_H 32

typedef struct {
    int x, y;
    int hot_x, hot_y;
    int width, height;
    unsigned int* buffer; // RGBA ou ARGB dependendo do seu formato
    int visible;
} mouse_cursor_t;

extern mouse_cursor_t g_mouse_cursor;
void mouse_cursor_init();
void set_cursor(int x, int y);

#endif