#include <procyon/procyon.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "file_manager.h"

#define FM_WINDOW_X 60
#define FM_WINDOW_Y 40
#define FM_WINDOW_WIDTH 800
#define FM_WINDOW_HEIGHT 600

int main(void)
{
    int fd = open("/dev/pts/0", O_RDWR);
    if(fd >= 0)
        dup2(fd, STDOUT_FILENO);

    int socket_fd = -1;
    int window_registered = 0;
    int exit_code = 1;

    window_t *window = NULL;
    ui_context_t ui = {0};
    file_manager_t *manager = NULL;

    socket_fd = procyon_init_client();

    if (socket_fd < 0) {
        fprintf(stderr, "File Manager: nao foi possivel ligar ao servidor Procyon.\n");
        goto cleanup;
    }

    window = window_create(
        FM_WINDOW_X,
        FM_WINDOW_Y,
        FM_WINDOW_WIDTH,
        FM_WINDOW_HEIGHT,
        "File Manager",
        argb(255, 245, 245, 245),
        0xFF202020
    );

    if (!window) {
        fprintf(stderr, "File Manager: falha ao criar a janela.\n");
        goto cleanup;
    }

    ui.fd = socket_fd;
    ui.window = window;
    ui.components = NULL;
    ui.count = 0;
    ui.modal_active = 0;
    ui.modal_owner = NULL;
    ui.sub_modal_owner = NULL;
    ui.lock = 0;

    window_controls_t controls;
    window_controls_init(
        &ui,
        window,
        &controls,
        WINDOW_CONTROLS_ALL);

    manager = file_manager_create(
        &ui,
        window->client.x,
        window->client.y,
        window->client.width,
        window->client.height,
        "/",
        FILE_MANAGER_MODE,
        NULL,
        0
    );

    

    if (!manager) {
        fprintf(stderr, "File Manager: falha ao inicializar os componentes.\n");
        goto cleanup;
    }

    if (enviar_janela_ao_servidor(socket_fd, window, 1) != 0) {
        fprintf(stderr, "File Manager: falha ao registar a janela no servidor.\n");
        goto cleanup;
    }

    window_registered = 1;
    exit_code = 0;

    for (;;) {
        ui_draw(&ui);

        event_t event = get_next_event_window(window);

        ui_event(&ui, event);

        if (event.type == EVENT_WINDOW_CLOSE)
            break;
    }

cleanup:
    if (window_registered) {
        if (enviar_remocao_janela(socket_fd, window) == 0)
            window_destroy(window);
    } else if (window) {
        window_destroy(window);
    }

    if (socket_fd >= 0)
        procyon_finish_client(socket_fd);

    return exit_code;
}
