/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: cat.c
 *    Description: Utilitário para ler, concatenar e exibir o conteúdo de
 *                 ficheiros na saída padrão (STDOUT) utilizando a API POSIX.
 * 
 *         Author: Nelson Cole
 *   Created Date: 06/10/2026
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 4096
#endif

// Função auxiliar para processar e imprimir um descritor de ficheiro aberto
static void cat_stream(int fd, const char *filename) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Loop de leitura: consome blocos até atingir o Fim de Ficheiro (EOF = 0)
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        ssize_t bytes_written = write(STDOUT_FILENO, buffer, bytes_read);
        
        // Proteção contra falhas de escrita (ex: terminal cheio ou interrupção)
        if (bytes_written < 0) {
            fprintf(stderr, "cat: erro de escrita na saida padrao\n");
            _exit(1);
        }
    }

    // Se o retorno da leitura for negativo, ocorreu um erro real de I/O (-EIO, etc.)
    if (bytes_read < 0) {
        fprintf(stderr, "cat: erro ao ler o ficheiro '%s'\n", filename ? filename : "stdin");
    }
}

int main(int argc, char *argv[]) {
    // 1. COMPORTAMENTO POSIX: Se não forem passados argumentos, lê diretamente de STDIN
    if (argc < 2 || (argc == 2 && strcmp(argv[1], "-") == 0)) {
        cat_stream(STDIN_FILENO, NULL);
        _exit(0);
    }

    // 2. Processa cada um dos ficheiros fornecidos na linha de comandos
    for (int i = 1; i < argc; i++) {
        // Se o nome do ficheiro for "-", lê do STDIN interativamente naquela posição
        if (strcmp(argv[i], "-") == 0) {
            cat_stream(STDIN_FILENO, NULL);
            continue;
        }

        // Abre o ficheiro em modo estrito de leitura (O_RDONLY)
        int fd = open(argv[i], O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "cat: %s: Ficheiro nao encontrado ou sem permissoes de leitura\n", argv[i]);
            continue;
        }

        cat_stream(fd, argv[i]);
        printf("\n");
        close(fd);
    }

    _exit(0);
    return 0;
}