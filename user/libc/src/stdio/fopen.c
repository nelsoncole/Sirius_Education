#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>


/*
 *  Modo 	Significado
 *
 * r		Abre um arquivo-texto para leitura
 * w		Cria um arquivo-texto para escrita
 * a		Anexa a um arquivo-texto
 * rb		Abre um arquivo binário para leitura
 * wb		Cria um arquivo binário para escrita
 * ab		Anexa a um arquivo binário
 * r+		Abre um arquivo-texto para leitura/escrita
 * w+		Cria um arquivo-texto para leitura/escrita
 * a+		Anexa ou cria um arquivo-texto para leitura/escrita
 * r+b		Abre um arquivo binário para leitura/escrita
 * w+b		Cria um arquivo binário para escrita/escrita
 * a+b		Anexa ou cria um arquivo binário para escrita/escrita
 *
 *	
 */

FILE *fopen(const char *path, const char *mode) 
{
    if (mode == NULL || mode[0] == '\0') return NULL;

    int flags = 0;
    
    // 1. Converter a string de modo para as flags do open()
    if (mode[0] == 'r') {
        flags = O_RDONLY;
    } else if (mode[0] == 'w') {
        // O_TRUNC limpa o ficheiro se já existir; O_CREAT cria se não existir
        flags = O_WRONLY | O_CREAT | O_TRUNC; 
    } else if (mode[0] == 'a') {
        // O_APPEND escreve sempre no final do ficheiro
        flags = O_WRONLY | O_CREAT | O_APPEND;
    } else {
        return NULL; // Modo não suportado nesta implementação simples
    }

    // Se o modo incluir '+', abre em modo de leitura e escrita
    if (mode[1] == '+') {
        flags = (flags & ~(O_RDONLY | O_WRONLY)) | O_RDWR;
    }

    // 2. Chamar o open do sistema
    int fd = open(path, flags);
    if (fd == -1) {
        return NULL; // Falha ao abrir o ficheiro
    }

    // 3. Associar o descritor de ficheiro (fd) a um fluxo FILE *
    FILE *fp = fdopen(fd, mode);
    if (fp == NULL) {
        // Se fdopen falhar, fechamos o fd para evitar fuga de memória
        close(fd); 
    }

    return fp;
}
