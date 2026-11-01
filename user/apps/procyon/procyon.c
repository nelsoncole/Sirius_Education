/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: procyon.c
 *    Description: Procyon Display Server & Compositor (Ring 3).
 *                 Inicializa o servidor via socket local (AF_LOCAL/AF_UNIX) para 
 *                 comunicação com clientes gráficos e faz a gestão do Framebuffer
 *                 através do subsistema TTY do kernel.
 * 
 *         Author: Nelson Cole
 *   Created Date: 01/11/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 01/11/2026
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

#define PROCYON_SOCKET_PATH "/dev/procyon.sock"

struct tty_user {
    uint64_t user_buffer;
    uint64_t size;
    uint32_t width;                 
    uint32_t height;                
    uint32_t ppsl;                  
    uint32_t bpp;   
};

int main() {
    
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

    int tty = open("/dev/tty2", O_RDWR);
    if (tty < 0) {
        printf("Erro ao abrir /dev/tty2\n");
        close(server_fd);
        return -1;
    }

    struct tty_user *info = malloc(sizeof(struct tty_user));
    if (ioctl(tty, TTY_IOCTL_VIDEO_INFO, info) < 0) {
        printf("Erro ao obter informacoes de video\n");
        free(info);
        close(tty);
        close(server_fd);
        return -1;
    }

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

    for (uint32_t i = 0; i < fb_pixels; i++) {
        compositor_buffer[i] = 0x002B36; 
    }

    info->size = fb_pixels * sizeof(unsigned int);
    info->user_buffer = (uint64_t)compositor_buffer;
    ioctl(tty, TTY_IOCTL_VIDEO_WRITE, info);

    while (1) {
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(struct sockaddr_un);
        
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            continue;
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