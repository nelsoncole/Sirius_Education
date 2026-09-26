#include <stdio.h>

/**
 * clearerr - Limpa os indicadores de erro e de fim de ficheiro de um fluxo.
 * @stream:  Ponteiro para a estrutura FILE que terá o estado redefinido.
 */
void clearerr(FILE *stream)
{
    if (stream == NULL) {
        return;
    }

    // Desliga de forma atómica/direta os bits de erro e EOF,
    // preservando intactas as flags de modo de acesso (_IOREAD, _IOWRT, _IOBIN)
    stream->flags &= ~(_IOEOF | _IOERR);
}