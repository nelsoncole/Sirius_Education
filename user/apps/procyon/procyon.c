/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: procyon.c
 *    Description: Procyon Display Server & Compositor (Userspace - Ring 3).
 * 
 *                 Atua como Servidor Gráfico centralizado e Compositor de janelas.
 *                 
 *                 Mecânicas Principais:
 *                 1. IPC (Inter-Process Communication): Inicializa um socket de
 *                    domínio Unix local (AF_LOCAL/AF_UNIX) para receber comandos,
 *                    eventos de input e referências de memória partilhada (shm)
 *                    dos clientes gráficos (aplicações).
 *                 2. Gestão de Framebuffer (Kernel TTY): Interage diretamente 
 *                    com o subsistema TTY do Kernel em modo utilizador através de 
 *                    `ioctl()`, mapeando o hardware de vídeo e enviando o Backbuffer 
 *                    composto para renderização física.
 *                 3. Arquitetura Wayland-like: O servidor atua estritamente como
 *                    um coordenador. Os clientes desenham nos seus próprios buffers
 *                    privados e o Procyon realiza o Blitting e a composição final
 *                    por camadas (layers).
 * 
 *         Author: Nelson Cole
 *   Created Date: 01/11/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 02/11/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <pthread.h>

#include "cpu_memory.h"
#include "window.h"
#include "compositor.h"
#include "mouse_cursor.h"
#include "event.h"
#include "procyon_ipc.h"

struct tty_user {
    uint64_t user_buffer;
    uint64_t size;
    uint32_t x_start;
    uint32_t y_start;
    uint32_t width;                 
    uint32_t height;                
    uint32_t ppsl;                  
    uint32_t bpp;   
};

struct tty_user *info;
int tty;

// obrigatorio definir
unsigned int* back_buffer;
int screen_width;
int screen_height;
int pitch;

extern void procyon_desktop_main(int info_width, int info_height);

#define MOUSE_BUTTON_LEFT (1 << 0)   /* Bit 0: Botão Esquerdo Pressionado */
#define MOUSE_BUTTON_RIGHT (1 << 1)  /* Bit 1: Botão Direito Pressionado */
#define MOUSE_BUTTON_MIDDLE (1 << 2) /* Bit 2: Botão do Meio Pressionado */

typedef struct
{
    uint8_t buttons; /* Bitmask com os estados dos botões */
    int16_t delta_x; /* Deslocamento relativo no eixo X */
    int16_t delta_y; /* Deslocamento relativo no eixo Y */
} mouse_packet_t;

mouse_packet_t packet;

static pthread_t thread_mouse;
void *thread_mouse_func(void *arg)
{
    (void)arg;
    //int buffer_size = g_mouse_cursor.width * g_mouse_cursor.height * sizeof(unsigned int);
    //unsigned int *buffer = malloc(buffer_size);
    //struct tty_user *info_2 = malloc(sizeof(struct tty_user));
    //memcpy(info_2, info, sizeof(struct tty_user));

    int fd = open("/dev/mouse", O_RDONLY);

    int x = 0;
    int y = 0;

    if (fd >= 0)
    {
        while (1)
        {
            int ret = read(fd, &packet, sizeof(packet));

            if (ret == sizeof(packet))
            {
                //printf("%dx%x %x", packet.delta_x, packet.delta_y, packet.buttons);
                x += packet.delta_x; 
                y += packet.delta_y;

                if (x < 0) x = 0;
                if (y < 0) y = 0;

                set_cursor(x, y);

                handle_mouse_event(x, y, packet.buttons);

                /*
                compositor_draw_cursor_2(buffer);
                
                info_2->x_start = g_mouse_cursor.x;
                info_2->y_start = g_mouse_cursor.y;
                info_2->width = g_mouse_cursor.width;
                info_2->height = g_mouse_cursor.height;
                info_2->ppsl = info_2->width;
                info_2->size = buffer_size;
                info_2->user_buffer = (uint64_t)buffer;
                ioctl(tty, TTY_IOCTL_VIDEO_WRITE, info_2);*/

                //compositor_draw_cursor(back_buffer);
                
                compositor_render();
                ioctl(tty, TTY_IOCTL_VIDEO_WRITE, info);
            }
        }
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    memcpy_init();

    
    int server_fd, client_fd;
    struct sockaddr_un server_addr;

    server_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("Erro ao criar socket AF_LOCAL\n");
        return -1;
    }


    unlink(PROCYON_SOCKET_PATH);

    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_LOCAL;
    strcpy(server_addr.sun_path, PROCYON_SOCKET_PATH);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un)) < 0) {
        printf("Erro no bind do socket local\n");
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 5) < 0) {
        printf("Erro no listen do socket local\n");
        close(server_fd);
        return -1;
    }

    tty = open("/dev/tty2", O_RDWR);
    if (tty < 0) {
        printf("Erro ao abrir /dev/tty2\n");
        close(server_fd);
        return -1;
    }

    info = malloc(sizeof(struct tty_user));
    if (ioctl(tty, TTY_IOCTL_VIDEO_INFO, info) < 0) {
        printf("Erro ao obter informacoes de video\n");
        free(info);
        close(tty);
        close(server_fd);
        return -1;
    }

    info->x_start = 0;
    info->y_start = 0;

    printf("Procyon inicializado. Width: %d | Height: %d | Bpp: %d\n", 
           info->width, info->height, info->bpp);

    uint32_t fb_pixels = info->width * info->height;
    unsigned int *compositor_buffer = malloc(fb_pixels * sizeof(unsigned int));
    if (!compositor_buffer) {
        printf("Erro ao alocar o buffer do compositor\n");
        free(info);
        close(tty);
        close(server_fd);
        return -1;
    }

    info->size = fb_pixels * sizeof(unsigned int);
    info->user_buffer = (uint64_t)compositor_buffer;

    mouse_cursor_init();
    
    pthread_create(&thread_mouse, NULL, thread_mouse_func, NULL);

    // Init
    back_buffer = compositor_buffer;
    screen_width = info->width;
    screen_height = info->height;
    pitch = screen_width * (info->bpp/8);

    pid_t pid_desktop = fork();

    if( pid_desktop == 0) {
        procyon_desktop_main(screen_width, screen_height);
        _exit(0);
    }

    while (1)
    {
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(struct sockaddr_un);

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            continue;
        }

        procyon_msg_header_t header;
        if (read(client_fd, &header, sizeof(procyon_msg_header_t)) > 0)
        {
            if (header.command == PROCYON_CMD_ADD_WINDOW)
            {
                ipc_msg_add_window_t msg;

                if (read(client_fd, &msg, sizeof(ipc_msg_add_window_t)) > 0)
                {
                    window_t *wn = (window_t *)msg.shm_win;

                    if (wn)
                    {
                        compositor_add_window(wn, msg.layer);
                        procyon_msg_response_t response;
                        response.status = 0;
                        write(client_fd, &response, sizeof(response));

                        if (wn->visible)
                        {
                            compositor_render();
                            ioctl(tty, TTY_IOCTL_VIDEO_WRITE, info);
                        }
                    }
                }
            }
            else if (header.command == PROCYON_CMD_ADD_WINDOW_CHILD)
            {
                ipc_msg_add_window_child_t msg;

                if (read(client_fd, &msg, sizeof(msg)) ==
                    sizeof(msg))
                {
                    window_t *win = (window_t *)msg.shm_win;
                    window_t *parent = (window_t *)msg.shm_parent;

                    if (win && parent)
                    {
                        compositor_add_window_child(win, parent);
                        procyon_msg_response_t response;
                        response.status = 0;
                        write(client_fd, &response, sizeof(response));

                        if (win->visible)
                        {
                            compositor_render();
                            ioctl(tty, TTY_IOCTL_VIDEO_WRITE, info);
                        }
                    }
                }
            }
            else if (header.command == PROCYON_CMD_REMOVE_WINDOW_CHILD)
            {
                ipc_msg_remove_window_child_t msg;

                if (read(client_fd, &msg, sizeof(msg)) ==
                    sizeof(msg))
                {
                    window_t *win = (window_t *)msg.shm_win;

                    if (win)
                    {
                        compositor_remove_window_child(win);
                        procyon_msg_response_t response;
                        response.status = 0;
                        write(client_fd, &response, sizeof(response));

                        compositor_render();
                        ioctl(tty, TTY_IOCTL_VIDEO_WRITE, info);
                    }
                }
            }
            else if (header.command == PROCYON_CMD_INVALIDATE)
            {
                ipc_msg_invalidate_t msg;

                if (read(client_fd, &msg, sizeof(ipc_msg_invalidate_t)) > 0)
                {
                    window_t *wn = (window_t *)msg.shm_win;
                    if (wn)
                    {
                        /* Ativa o sinalizador de modificação na janela mapeada */
                        wn->invalid = 1;
                    }
                }

                procyon_msg_response_t response;
                response.status = 0;
                write(client_fd, &response, sizeof(response));

                /* Renderiza as alterações das camadas e atualiza o ecrã físico */
                compositor_render();
                ioctl(tty, TTY_IOCTL_VIDEO_WRITE, info);
            }
        }
        close(client_fd);
    }

    free(compositor_buffer);
    free(info);
    close(tty);
    close(server_fd);
    unlink(PROCYON_SOCKET_PATH);

    return 0;
}