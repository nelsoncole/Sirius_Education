#include "icon.h"
#include "image.h"
#include "draw_rect.h"
#include "text.h"
#include "color.h"
#include <stdlib.h>
#include <string.h>
#include "event.h"

desktop_icon_t *create_desktop_icon(
    int x, int y, int width, int height, 
    unsigned char* image, const char *label, stbtt_fontinfo *font, 
    int font_size, int n_of_click, void (*func)()
) {
    desktop_icon_t *icon = malloc(sizeof(desktop_icon_t));
    icon->type = 1;
    icon->font = font;
    icon->font_size = font_size;
    icon->full_x = x;
    icon->full_y = y;
    icon->full_width = width + (font_size*2) + 8;
    icon->full_height = height + font_size + (font_size/2) + 8;
    icon->x = icon->full_x + font_size + 4;
    icon->y =  icon->full_y + (font_size/4) + 4;
    icon->width = width;
    icon->height = height;
    icon->image = image;
    icon->selected = 0;
    icon->hovered = 0;
    icon->render = 1;
    icon->hovered_render = 0;
    icon->on_click = func;
    icon->click_count = 0;
    icon->clock = 0;
    icon->num_of_click = n_of_click;
    icon->bg_backup = malloc(icon->full_width*icon->full_height*4);
    strncpy(icon->label, label ? label : "", sizeof(icon->label) - 1);
    icon->label[sizeof(icon->label) - 1] = '\0';

    icon->fg = rgb(240, 240, 240);
    icon->bg = 0;
    icon->hover_color = RGBA_TO_ARGB(rgba(0, 128, 255, 128));
    icon->selected_color = RGBA_TO_ARGB(rgba( 0, 0, 255, 128));
    return icon;
}

desktop_icon_t *create_icon_button(
    int x, int y, int width, int height, int full_width, 
    unsigned char* image, const char *label, stbtt_fontinfo *font, 
    int font_size, int n_of_click, void (*func)()
) {
    desktop_icon_t *icon = malloc(sizeof(desktop_icon_t));
    icon->type = 2;
    icon->font = font;
    icon->font_size = font_size;
    icon->full_x = x;
    icon->full_y = y;
    icon->full_width = full_width;
    icon->full_height = height;
    icon->x = icon->full_x;
    icon->y = icon->full_y;
    icon->width = width;
    icon->height = height;
    icon->image = image;
    icon->selected = 0;
    icon->hovered = 0;
    icon->render = 1;
    icon->hovered_render = 0;
    icon->on_click = func;
    icon->click_count = 0;
    icon->clock = 0;
    icon->num_of_click = n_of_click;
    icon->bg_backup = malloc(icon->full_width*icon->full_height*4);
    strncpy(icon->label, label ? label : "", sizeof(icon->label) - 1);
    icon->label[sizeof(icon->label) - 1] = '\0';

    icon->fg = rgb(240, 240, 240);
    icon->bg = 0;
    icon->hover_color = RGBA_TO_ARGB(rgba(0, 128, 255, 128));
    icon->selected_color = RGBA_TO_ARGB(rgba( 0, 0, 255, 128));
    return icon;
}


// Verificando hover
int is_mouse_over_icon(int mouse_x, int mouse_y, desktop_icon_t *icon) {
    return mouse_x >= icon->full_x && mouse_x < icon->full_x + icon->full_width &&
           mouse_y >= icon->full_y && mouse_y < icon->full_y + icon->full_height;
}

void handle_mouse_click(desktop_icon_t** icons, int num_icons, event_t ev) {
    for (int i = 0; i < num_icons; i++) {
        desktop_icon_t* icon = icons[i];

        if (ev.mouse.x >= icon->full_x && ev.mouse.x < icon->full_x + icon->full_width &&
            ev.mouse.y >= icon->full_y && ev.mouse.y < icon->full_y + icon->full_height) {
            // Marca este como selecionado
            if(ev.type == EVENT_MOUSE_DOWN) {
                icon->selected = 1;
                // Deseleciona os outros
                for (int j = 0; j < num_icons; j++) {
                    if (j != i) icons[j]->selected = 0;
                }
            } else if (ev.type == EVENT_MOUSE_UP)
            {
                icon->selected = 0;

                // Tempo de click
                unsigned long new = ev.clock;

                if(!icon->clock) {
                    icon->clock = new;
                    icon->click_count++;
                }else if((float)((new - icon->clock) /10) <= 2.0){
                    // Clique dentro de 2 segundos
                    icon->click_count++;
                    icon->clock = 0;
                }else {
                    icon->click_count = 0;
                    icon->clock = 0;
                }
                

                // Chama a função associada ao clique, se existir
                if (icon->on_click && icon->click_count >= icon->num_of_click) {
                    icon->on_click();
                    icon->click_count = 0;
                    icon->clock = 0;
                }
            }

            return;
        }
    }
}

void render_desktop_icon(desktop_icon_t *icon, int mouse_x, int mouse_y, window_t* win) {

    int hovered = is_mouse_over_icon(mouse_x - win->x, mouse_y - win->y, icon);

    unsigned int icon_color = 0xDDDDDD;

    if (icon->selected && hovered) {
        if(!icon->hovered_render) {
            if(icon->hovered){
                restore_icon_background(icon, win->buffer, win->width); // recupera fundo
            }

            backup_icon_background(icon, win->buffer, win->width); // salva fundo

            icon_color = icon->selected_color;
            draw_rect_alpha(win, icon->full_x, icon->full_y, icon->full_width, icon->full_height, icon_color);
            icon->hovered = 0;
            icon->hovered_render = 1;
            icon->render = 1;
        }
    }
    else if (hovered) {
        if (!icon->hovered) {
            // Só desenha o highlight na entrada
            icon->hovered = 1;
            icon->render = 1;

            if(icon->hovered_render) {
                restore_icon_background(icon, win->buffer, win->width); // recupera fundo
            }
            
            backup_icon_background(icon, win->buffer, win->width); // salva fundo
            
            icon->hovered_render = 0;
            // Desenha um retângulo azul 25% opaco
            icon_color = icon->hover_color;// hover_color;
            draw_rect_alpha(win, icon->full_x, icon->full_y, icon->full_width, icon->full_height, icon_color);
        }
        // Se já estava em hover, não redesenha — mantém estado
    } else {
        if (icon->hovered || icon->hovered_render) {
            // Mouse saiu — reset e redesenho
            icon->hovered = 0;
            icon->hovered_render = 0;
            restore_icon_background(icon, win->buffer, win->width); // recupera fundo
        }
    }

    if(icon->render) {
        icon->render = 0;
        // Sempre desenha o ícone com a cor atual
        if(icon->image) {
            draw_image_tint(
                icon->x, icon->y, win->width, win->height,
                icon->image, icon->width, icon->height,
                win->buffer, icon_color);            
        }
        //
        if(icon->label[0]) {
            if(icon->type == 1){
                draw_text_centered(icon->font[0], icon->label, icon->full_x, icon->full_y + icon->height+4, 
                icon->full_width, icon->fg, icon->font_size, win);

            }else if(icon->type == 2){
                draw_text(icon->font[0], icon->label, icon->x+icon->width+10, icon->full_y + ((icon->full_height/2)-(icon->font_size/2)), 
                icon->full_width, icon->fg, icon->font_size, win);
            }
        }

    }
    
}


void backup_icon_background(desktop_icon_t* icon, unsigned int* framebuffer, int screen_width) {
    if (!icon->bg_backup) {
        int size = icon->full_width * icon->full_height;
        icon->bg_backup = malloc(sizeof(unsigned int) * size);
    }

    for (int y = 0; y < icon->full_height; ++y) {
        for (int x = 0; x < icon->full_width; ++x) {
            int src_index = (icon->full_y + y) * screen_width + (icon->full_x + x);
            int dst_index = y * icon->full_width + x;
            icon->bg_backup[dst_index] = framebuffer[src_index];
        }
    }
}

void restore_icon_background(desktop_icon_t* icon, unsigned int* framebuffer, int screen_width) {
    if (!icon->bg_backup) return;

    for (int y = 0; y < icon->full_height; ++y) {
        for (int x = 0; x < icon->full_width; ++x) {
            int dst_index = (icon->full_y + y) * screen_width + (icon->full_x + x);
            int src_index = y * icon->full_width + x;
            framebuffer[dst_index] = icon->bg_backup[src_index];
        }
    }
}



desktop_icon_t *create_icon_from_file(
    const char* filepath,
    int x, int y,
    int width, int height,
    const char* label,
    stbtt_fontinfo* font,
    int font_size,
    int n_of_click,
    void (*on_click)()
) {
    image_t img;
    unsigned char* pixels = NULL;

    if (!load_image(filepath, &img)) {
        pixels = draw_image_resize(img.pixels, img.width, img.height, width, height);
        image_free(&img);
    }

    return create_desktop_icon(
        x,
        y,
        width,
        height,
        pixels,
        label,
        font,
        font_size,
        n_of_click,
        on_click
    );
}

desktop_icon_t *create_icon_buttun_from_file(
    const char* filepath,
    int x, int y,
    int width, int height, int full_width, 
    const char* label,
    stbtt_fontinfo* font,
    int font_size,
    int n_of_click,
    void (*on_click)()
) {
    image_t img;
    unsigned char* pixels = NULL;

    if (!load_image(filepath, &img)) {
        pixels = draw_image_resize(img.pixels, img.width, img.height, width, height);
        image_free(&img);
    }

    return create_icon_button(
        x,
        y,
        width,
        height,
        full_width,
        pixels,
        label,
        font,
        font_size,
        n_of_click,
        on_click
    );
}