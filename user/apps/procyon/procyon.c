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

#include "cpu_memory.h"
#include "pixel.h"
#include "window.h"
#include "text.h"
#include "image.h"
#include "compositor.h"

#define PROCYON_SOCKET_PATH "/dev/procyon.sock"

struct tty_user {
    uint64_t user_buffer;
    uint64_t size;
    uint32_t width;                 
    uint32_t height;                
    uint32_t ppsl;                  
    uint32_t bpp;   
};

// obrigatorio definir
unsigned int* back_buffer;
int screen_width;
int screen_height;
int pitch;

extern int __sys_active_tty();

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    
    //__sys_active_tty();

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

    // Init
    back_buffer = compositor_buffer;
    screen_width = info->width;
    screen_height = info->height;
    pitch = screen_width * (info->bpp/8);

    window_t* back = overlay_window_create(0, 0, info->width, info->height, 0x000000, 0x0000FF);
    if(back) {

        image_t img;
        if (!load_image("usr/share/wallpaper/sirius.png", &img))
        {
            unsigned char *img_data = draw_image_resize(img.pixels, img.width, img.height,
                                                        back->width, back->height);

            draw_image(0, 0, back->width, back->height,
                       img_data, back->width, back->height, back->buffer);

            image_free(&img);
            free(img_data);
        }

       compositor_add_window(back, 0); 
    }

    window_t* wn = window_create(300, 500, 400, 200, "Test", 0x000000, 0x0000FF);
    if(wn) {
        
        if(wn->free_font_buffer) {
            const char *texto =
                "Bem-vindo ao Sirius OS!\n"
                "Um sistema operativo desenvolvido em C.\n"
                "Arquitetura x86_64 com suporte SMP.\n"
                "Multitarefa, memoria virtual e interface grafica.\n"
                "Desenvolvido para oferecer desempenho e estabilidade.\n"
                "Sirius OS - O futuro começa aqui!";
            draw_text_centered_wrapped(*(stbtt_fontinfo*)wn->font, texto, wn->client.x, wn->client.y, wn->client.width, wn->client.height, 0xFFFFFFFF, 18, wn);
        }
        compositor_add_window(wn, 1);
    }

    compositor_render(); 

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