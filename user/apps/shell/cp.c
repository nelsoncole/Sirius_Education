/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: cp.c
 *    Description: Utilitário para cópia de ficheiros (cp) em espaço de utilizador.
 *                 Lê o ficheiro de origem e escreve no destino utilizando buffers
 *                 de paginação padrão (4KB) nativos da libc.
 * 
 *         Author: Nelson Cole
 *   Created Date: 04/10/2026
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[])
{
    // 1. Validação padrão de argumentos da linha de comandos
    if (argc != 3) {
        fprintf(stderr, "cp: argumentos invalidos\n");
        fprintf(stderr, "Utilizacao: %s <origem> <destino>\n", "cp");
        exit(1);
    }

    const char *src_path = argv[1];
    const char *dst_path = argv[2];

    // 2. Abre o ficheiro de origem para leitura
    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        fprintf(stderr, "cp: nao foi possivel abrir o ficheiro de origem '%s'\n", src_path);
        exit(1);
    }

    // 3. Cria ou trunca o ficheiro de destino para escrita (Permissão padrão 0644)
    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        fprintf(stderr, "cp: nao foi possivel criar o ficheiro de destino '%s'\n", dst_path);
        close(src_fd);
        exit(1);
    }

    // 4. Alocação do buffer dinâmico na Heap da libc do Sirius OS
    char *buffer = (char *)malloc(BUFFER_SIZE);
    if (buffer == NULL) {
        fprintf(stderr, "cp: falha de memoria ao alocar buffer de copia\n");
        close(src_fd);
        close(dst_fd);
        exit(1);
    }

    ssize_t bytes_read;
    ssize_t bytes_written;
    int erro_escrita = 0;

    // 5. Ciclo de leitura e escrita (I/O Multiplexado em blocos de 4KB)
    while ((bytes_read = read(src_fd, buffer, BUFFER_SIZE)) > 0) 
    {
        bytes_written = write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) 
        {
            fprintf(stderr, "cp: erro critico de escrita no destino ou disco cheio\n");
            close(src_fd);
            close(dst_fd);
            exit(1);
        }
    }

    if (bytes_read < 0) {
        fprintf(stderr, "cp: erro ao ler o ficheiro '%s'\n", src_path);
    }

    // 6. Limpeza e encerramento seguro dos descritores de ficheiro
    free(buffer);
    close(src_fd);
    close(dst_fd);

    // Se houve falha física durante a cópia, retorna erro
    if (bytes_read < 0 || erro_escrita) {
        exit(1);
    }

    printf("cp: '%s' copiado para '%s' com sucesso.\n", src_path, dst_path);
    exit(0);
}