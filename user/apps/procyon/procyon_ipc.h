#ifndef PROCYON_IPC_H
#define PROCYON_IPC_H

#include <stdint.h>

#include "window.h"

#define PROCYON_SOCKET_PATH "/dev/procyon.sock"

#define PROCYON_CMD_ADD_WINDOW          0x01
#define PROCYON_CMD_ADD_WINDOW_CHILD    0x02
#define PROCYON_CMD_REMOVE_WINDOW_CHILD 0x03
#define PROCYON_CMD_DESTROY_WINDOW      0x04
#define PROCYON_CMD_INVALIDATE          0x05 /* Cliente avisa que redesenhou e quer um refresh */

/* Cabeçalho de qualquer mensagem */
typedef struct {
    uint32_t command;       /* Ex: PROCYON_MSG_ADD_WINDOW */
    uint32_t size;          /* Tamanho total do payload */
} procyon_msg_header_t;

/* Dados necessários para o compositor registar a janela (Otimizado) */
typedef struct {
    uint32_t layer;   /* O destino (ex: 1 para janelas normais) */
    uint64_t shm_win; /* Endereço ou ID da estrutura window_t partilhada */
} ipc_msg_add_window_t;

/* Payload para avisar que a janela precisa de redesenho */
typedef struct {
    uint64_t shm_win; /* Endereço da janela que modificou o buffer */
} ipc_msg_invalidate_t;

typedef struct {
    uint64_t shm_win;
    uint64_t shm_parent;
} ipc_msg_add_window_child_t;

typedef struct {
    uint64_t shm_win;
} ipc_msg_remove_window_child_t;

typedef struct {
    int32_t status;
} procyon_msg_response_t;


int procyon_init_client(void);
int procyon_finish_client(int socket_fd);
int enviar_janela_ao_servidor(int socket_fd, window_t* wn, uint32_t layer);
int avisar_redesenho_ao_servidor(int socket_fd, window_t* wn);
int enviar_janela_filha_ao_servidor(int socket_fd, window_t *win, window_t *parent);
int enviar_remocao_janela_filha(int socket_fd, window_t *win);

#endif
