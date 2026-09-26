#include <stdio.h>
#include <unistd.h>

#include <stdio.h>
#include <unistd.h>

int fgetc(FILE *fp)
{
    if (fp == NULL) {
        return EOF;
    }

    // CORREÇÃO 1: Inicializa a variável para calar o warning [-Wmaybe-uninitialized]
    unsigned char ch = 0; 

    // CORREÇÃO 2: Usa read() em vez de write() para capturar o caractere do VFS
    if (read(fp->fd, &ch, 1) == 1) 
    {
        return (int)ch; // Sucesso: retorna o caractere lido casted para int
    }

    return EOF; // Falha na leitura ou Fim do Ficheiro (EOF)
}