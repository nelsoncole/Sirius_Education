#ifndef ICON_H
#define ICON_H

#include "window.h"
#include "text.h"

typedef struct {
    int type;                   // 1 = icon desktop, 2 = icon buttun
    int x, y;                   // Posição na tela
    int width, height;          // Tamanho do ícone
    unsigned char* image;       // Dados do ícone (RGBA)
    char label[256];            // Nome ou descrição opcional
    int selected;               // 1 se estiver clicado
    int hovered;                // 1 se estiver o cursor
    int hovered_render;         // 1 se ja renderizou
    void (*on_click)();         // Ponteiro para função de ação
    int click_count;
    unsigned long clock;
    int num_of_click;
    unsigned int* bg_backup;    // buffer para salvar o fundo original
    // Área total do componente (imagem + texto)
    int full_x, full_y;             // Posição da área total
    int full_width, full_height;    // Tamanho da área totalo
    stbtt_fontinfo *font;
    int font_size;
    int render;

    unsigned int fg, bg;
    unsigned int hover_color;
    unsigned int selected_color;
} desktop_icon_t;

desktop_icon_t *create_desktop_icon(
    int x, int y, int width, int height, 
    unsigned char* image, const char *label, stbtt_fontinfo *font, 
    int font_size, int n_of_click, void (*func)()
);

desktop_icon_t *create_icon_button(
    int x, int y, int width, int height, int full_width,  
    unsigned char* image, const char *label, stbtt_fontinfo *font, 
    int font_size, int n_of_click, void (*func)()
);

int is_mouse_over_icon(int mouse_x, int mouse_y, desktop_icon_t *icon);

void handle_mouse_click(desktop_icon_t** icons, int num_icons, event_t ev);

void render_desktop_icon(desktop_icon_t *icon, int mouse_x, int mouse_y, window_t* win);

void backup_icon_background(desktop_icon_t* icon, unsigned int* framebuffer, int screen_width);
void restore_icon_background(desktop_icon_t* icon, unsigned int* framebuffer, int screen_width);

desktop_icon_t *create_icon_from_file(
    const char* filepath,
    int x, int y,
    int width, int height,
    const char* label,
    stbtt_fontinfo* font,
    int font_size,
    int n_of_click,
    void (*on_click)()
);

desktop_icon_t *create_icon_buttun_from_file(
    const char* filepath,
    int x, int y,
    int width, int height, int full_width,
    const char* label,
    stbtt_fontinfo* font,
    int font_size,
    int n_of_click,
    void (*on_click)()
);


#endif