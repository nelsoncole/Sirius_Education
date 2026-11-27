#include <procyon/procyon.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>

#include "file_manager.h"

#define FM_WINDOW_X 60
#define FM_WINDOW_Y 40
#define FM_WINDOW_WIDTH 700
#define FM_WINDOW_HEIGHT 500

extern volatile int g_final;

int main(int argc, char *argv[])
{
    if (argc < 3)
        return 1;

    char *endptr;

    errno = 0;
    unsigned long parent_window_id = strtoul(argv[1], &endptr, 16);

    if (errno != 0 || argv[1] == endptr || *endptr != '\0')
        return 1;

    errno = 0;
    uintptr_t shared_address = (uintptr_t)strtoul(argv[2], &endptr, 16);

    if (errno != 0 || argv[2] == endptr || *endptr != '\0')
        return 1;

    ui_save_shared_t *shared = (ui_save_shared_t *)shared_address;

    const char *initial_path = shared->initial_path;
    //const char *initial_filename = shared->initial_filename;
    char *selected_path = shared->selected_path;

    int fd = open("/dev/pts/0", O_RDWR);
    if(fd >= 0)
        dup2(fd, STDOUT_FILENO);

    int socket_fd = -1;
    int window_registered = 0;
    int exit_code = 1;

    window_t *parent = (window_t *)parent_window_id;
    window_t *window = NULL;
    ui_context_t ui = {0};
    file_manager_t *manager = NULL;

    socket_fd = procyon_init_client();

    if (socket_fd < 0) {
        fprintf(stderr, "File Manager Open: nao foi possivel ligar ao servidor Procyon.\n");
        goto cleanup;
    }

    window = window_create(
        FM_WINDOW_X,
        FM_WINDOW_Y,
        FM_WINDOW_WIDTH,
        FM_WINDOW_HEIGHT,
        "Abrir",
        argb(255, 245, 245, 245),
        0xFF202020
    );

    if (!window) {
        fprintf(stderr, "File Manager Open: falha ao criar a janela.\n");
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
        WINDOW_CONTROL_CLOSE);

    g_final = 0;

    manager = file_manager_create(
        &ui,
        window->client.x,
        window->client.y,
        window->client.width,
        window->client.height,
        initial_path,
        FILE_MANAGER_OPEN,
        selected_path, 
        PATH_MAX
    );

    if (!manager) {
        fprintf(stdout, "File Manager Open: falha ao inicializar os componentes.\n");
        goto cleanup;
    }
    
    if (enviar_janela_ao_servidor(socket_fd, window, 1) != 0) {
        fprintf(stderr, "File Manager: falha ao registar a janela no servidor.\n");
        goto cleanup;
    }

    /*if(enviar_janela_filha_ao_servidor(socket_fd, window, parent) != 0) {
        fprintf(stdout, "File Manager Open: falha ao registar a janela no servidor.\n");
        goto cleanup;
    }*/

    window_registered = 1;
    exit_code = 0;

    for (;;) {
        ui_draw(&ui);

        event_t event = get_next_event_window(window);

        ui_event(&ui, event);

        if (event.type == EVENT_WINDOW_CLOSE)
            break;

        if(g_final)
            break;
    }

    shared->result = 1;

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
