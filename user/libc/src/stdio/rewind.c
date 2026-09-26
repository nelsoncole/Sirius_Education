#include <stdio.h>
#include <unistd.h>

/**
 * rewind - Reposiciona o cursor do ficheiro no início.
 * @fp: Ponteiro para o fluxo FILE.
 */
void rewind(FILE *fp)
{
    if (fp == NULL) 
    {
        return;
    }

    // 1. Invoca a sua chamada de sistema lseek(fd, offset, whence)
    // Força o offset a 0 a partir do início do ficheiro (SEEK_SET)
    lseek(fp->fd, 0, SEEK_SET);

    // 2. Limpa os sinalizadores de erro internos do fluxo
    fp->flags &= ~(_IOEOF | _IOERR);

    // 3. Limpa o registo e o buffer de devolução de caracteres do ungetc
    fp->has_ungetc = 0;
    fp->ungetc_buf = 0;;
}
