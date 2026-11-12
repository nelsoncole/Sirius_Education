#include "procyon_ipc.h"
#include "window.h"
#include <stdint.h>

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static int socket_read_all(int fd, void *buffer, size_t size)
{
    uint8_t *p = buffer;

    while (size > 0) {
        int n = read(fd, p, size);

        if (n <= 0)
            return -1;

        p += n;
        size -= n;
    }

    return 0;
}

int procyon_init_client(void) {
    int client_fd;

    client_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (client_fd < 0) return -1;

    return client_fd;
}

int procyon_finish_client(int socket_fd) {
    if (socket_fd < 0) {
        return -1;
    }

    return close(socket_fd);
}

int enviar_janela_ao_servidor(int socket_fd, window_t* wn, uint32_t layer) {
    procyon_msg_header_t header;
    header.command = PROCYON_CMD_ADD_WINDOW;
    header.size = sizeof(ipc_msg_add_window_t);

    ipc_msg_add_window_t msg;
    msg.layer = layer;

    /* Envia o cast do buffer mmap() partilhado */
    msg.shm_win = (uint64_t)wn;

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_LOCAL;
    strncpy(server_addr.sun_path, PROCYON_SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) < 0) {
        return -1;
    }

    // Envia o cabeçalho e depois os dados pelo socket Unix
    write(socket_fd, &header, sizeof(procyon_msg_header_t));
    write(socket_fd, &msg, sizeof(ipc_msg_add_window_t));

    procyon_msg_response_t response;

     if (socket_read_all(socket_fd, &response, sizeof(response)) < 0)
        return -1;

    return response.status;
}


int avisar_redesenho_ao_servidor(int socket_fd, window_t* wn) {
    procyon_msg_header_t header;
    header.command = PROCYON_CMD_INVALIDATE;
    header.size = sizeof(ipc_msg_invalidate_t);

    ipc_msg_invalidate_t msg;
    msg.shm_win = (uint64_t)wn;

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_LOCAL;
    strncpy(server_addr.sun_path, PROCYON_SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) < 0) {
        return -1;
    }

    // Envia o cabeçalho e o payload pelo socket
    write(socket_fd, &header, sizeof(procyon_msg_header_t));
    write(socket_fd, &msg, sizeof(ipc_msg_invalidate_t));

    procyon_msg_response_t response;

     if (socket_read_all(socket_fd, &response, sizeof(response)) < 0)
        return -1;

    return response.status;
}

int enviar_janela_filha_ao_servidor(int socket_fd, window_t *win, window_t *parent)
{
    if (!win || !parent)
        return -1;

    procyon_msg_header_t header;
    header.command = PROCYON_CMD_ADD_WINDOW_CHILD;
    header.size = sizeof(ipc_msg_add_window_child_t);

    ipc_msg_add_window_child_t msg;
    msg.shm_win = (uint64_t)win;
    msg.shm_parent = (uint64_t)parent;

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_LOCAL;

    strncpy(server_addr.sun_path,
            PROCYON_SOCKET_PATH,
            sizeof(server_addr.sun_path) - 1);

    if (connect(socket_fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
        return -1;

    write(socket_fd, &header, sizeof(header));
    write(socket_fd, &msg, sizeof(msg));

    procyon_msg_response_t response;

     if (socket_read_all(socket_fd, &response, sizeof(response)) < 0)
        return -1;

    return response.status;
}

int enviar_remocao_janela_filha(int socket_fd, window_t *win)
{
    if (!win)
        return -1;

    procyon_msg_header_t header;
    header.command = PROCYON_CMD_REMOVE_WINDOW_CHILD;
    header.size = sizeof(ipc_msg_remove_window_child_t);

    ipc_msg_remove_window_child_t msg;
    msg.shm_win = (uint64_t)win;

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_LOCAL;

    strncpy(server_addr.sun_path,
            PROCYON_SOCKET_PATH,
            sizeof(server_addr.sun_path) - 1);

    if (connect(socket_fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
        return -1;

    write(socket_fd, &header, sizeof(header));
    write(socket_fd, &msg, sizeof(msg));

    procyon_msg_response_t response;

     if (socket_read_all(socket_fd, &response, sizeof(response)) < 0)
        return -1;
    
    return response.status;
}