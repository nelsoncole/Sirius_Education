#include <stdlib.h>
#include "procyon.h"
#include <stdio.h>

component_t* label;

void on_button_click() {
    char new_text[128];
    static int i = 1;
    sprintf(new_text, "Click nº %d", i++);
    label_set_text(label, new_text);
}

const char* cidades[] = {"Luanda", "Benguela", "Lubango", "Huambo", "Cabinda", "Moxico", "Uige", "Cunene"};

void on_listbox_select(int index, const char* item) {
    (void)index;
    label_set_text(label, item);
}

void procyon_desktop_main(int info_width, int info_height) {
    // Inicializa o descritor do cliente local
    int socket_fd = procyon_init_client();
    if (socket_fd < 0) {
        exit(-1);
    }

    window_t* desktop_win = overlay_window_create(0, 0, info_width, info_height, 0x000000, 0x0000FF);
    if (!desktop_win) {
        procyon_finish_client(socket_fd);
        exit(-1);
    }

    // Carrega o Wallpaper no buffer da janela
    image_t img;
    if (!load_image("usr/share/wallpaper/sirius.png", &img)) {
        unsigned char *img_data = draw_image_resize(img.pixels, img.width, img.height,
                                                    desktop_win->width, desktop_win->height);

        draw_image(0, 0, desktop_win->width, desktop_win->height,
                   img_data, desktop_win->width, desktop_win->height, desktop_win->buffer);

        image_free(&img);
        free(img_data);
    }

    // regista a janela na camada mais baixa (Layer 0)
    enviar_janela_ao_servidor(socket_fd, desktop_win, 0);

    window_t *win = window_create(300, 200, 400, 600, "Test", 0xCCCCCC, 0x000000);
    if (win)
    {

        ui_context_t ui = {.fd = socket_fd, .window = win };

        label = create_label(&ui, 0, 0, win->client.width, "Bem-vindo ao Sirius OS!", 0x000000);
        
        create_button(&ui, 10, 80, 80, 40, "TEST", on_button_click);
        create_iconbutton(&ui, 110, 80, 200, 40, "IconButton", "usr/share/icons/information.png", 32, on_button_click);

        create_checkbox(&ui, 10, 150, 100, 20, "Checkbox", 0, 0);

        create_radiobutton(&ui, 120, 150, 100, 20, "Vermelho", 1, 1,0);
        create_radiobutton(&ui, 120, 180, 100, 20, "Verde", 1, 0,0);
        create_radiobutton(&ui, 120, 210, 100, 20, "Azul", 1, 0,0);

        create_slider(&ui, 10, 250, 100, 20, 0, 100, 50, 0);

        component_t* g_progress = create_progressbar(&ui, 10, 280, 200, 10, 100);
        progressbar_set_value(g_progress, 50); // define 50%

        create_listbox(&ui, 10, 320, 200, 80, cidades, 8, on_listbox_select);
        create_combobox(&ui, 240, 320, 100, 24, cidades, 8, 0);
        
        // regista a janela na camada mais baixa (Layer 1)
        enviar_janela_ao_servidor(socket_fd, win, 1);

        while (1)
        {
            ui_draw(&ui);
            event_t ev = get_next_event_window(win);
            ui_event(&ui, ev);
        }
    }

    while (1) {
    }

    // Caso o loop quebre, limpa tudo
    procyon_finish_client(socket_fd);
    exit(0);
}