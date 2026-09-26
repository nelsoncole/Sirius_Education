#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stddef.h>
#include <string.h>


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

FILE *fdopen(int fd, const char *mode) 
{
    // 1. Validação básica de sanidade
    if (fd < 0 || mode == NULL || mode[0] == '\0') {
        return NULL;
    }

    // 2. Aloca a estrutura FILE no Heap do seu sistema
    FILE *fp = (FILE *)malloc(sizeof(FILE));
    if (fp == NULL) {
        return NULL; // Falha de memória (malloc falhou)
    }

    memset(fp, 0, sizeof(FILE));

    // 3. Inicializa os campos do FILE com o seu File Descriptor
    fp->fd = fd;
    fp->flags = 0; // Pode usar isto para rastrear se é binário, texto, etc.

    // Nota: inicializaria aqui os ponteiros de buffer

    return fp;
}