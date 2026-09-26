#include <stdio.h>


int ungetc(int c, FILE *fp)
{
    if (fp == NULL || c == EOF) 
    {
        return EOF;
    }

    // O padrão POSIX garante suporte a pelo menos 1 caractere de ungetc consecutivo
    if (fp->has_ungetc) 
    {
        return EOF; // Já existe um caractere na fila de ungetc
    }

    // Guarda o caractere de forma segura sem adulterar o buffer do VFS
    fp->ungetc_buf = c & 0xFF;
    fp->has_ungetc = 1;

    // Se o seu sistema rastreia EOF nas flags, o ungetc limpa o estado de EOF
    // fp->flags &= ~_IOEOF;

    return c;
}
