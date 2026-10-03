/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ls.c
 *    Description: Utilitário de listagem de ficheiros (ls) nativo para o SiriusOS.
 *                 Abstrai o mapa físico e lê diretorias virtuais via SYS_GETDENTS,
 *                 suportando nomes de ficheiros de tamanho dinâmico na Libc.
 * 
 *         Author: Nelson Cole
 *   Created Date: 29/09/2026
 *        License: MIT
 * ============================================================================
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/usyscall.h>

/* 
 * Estrutura nativa do teu VFS para listagem de diretoria com tamanho dinâmico.
 * Alinhada com a especificação canónica do teu Micronúcleo.
 */
struct sys_dirent
{
    uint64_t d_ino;          // Número único do Inode (específico do FS)
    uint64_t d_off;          // Próximo offset (índice seguinte na tabela do VFS)
    unsigned short d_reclen; // Tamanho total desta estrutura nesta iteração (com padding)
    unsigned char d_type;    // Tipo do nó (DT_DIR, DT_REG, etc.)
    char d_name[];           // Nome do elemento terminado em '\0' (tamanho dinâmico)
} __attribute__((packed));

void ls(const char *path)
{
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        fprintf(stderr, "ls: nao foi possivel abrir a diretoria '%s'\n", path);
        return;
    }

    size_t buf_size = 1024;
    struct sys_dirent *dirp = (struct sys_dirent *)malloc(buf_size);
    if (dirp == NULL) {
        fprintf(stderr, "ls: falha de alocacao de buffer de leitura\n");
        close(fd);
        return;
    }

    memset(dirp, 0, buf_size);
    int nread = (int)syscall3(SYS_GETDENTS, (uint64_t)fd, (uint64_t)dirp, buf_size);
    if (nread > 0) 
    {
        struct sys_dirent *d = dirp;
        
        while ((uint64_t)d < (uint64_t)dirp + nread) 
        {
            if (d->d_ino != 0 && strlen(d->d_name) > 0) {
                printf("%s  ", d->d_name);
            }

            d = (struct sys_dirent *)((char *)d + d->d_reclen);
        }
        printf("\n");
    }
    else if (nread < 0) {
        fprintf(stderr, "ls: erro ao ler as entradas do VFS (Erro: %d)\n", nread);
    }

    free(dirp);
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        ls(".");
    } 
    else {
        for (int i = 1; i < argc; i++) {
            if (argc > 2) {
                printf("%s:\n", argv[i]);
            }
            ls(argv[i]);
            if (i < argc - 1) printf("\n");
        }
    }
    
    exit(0);
}