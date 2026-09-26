#include <stdio.h>

/**
 * ferror - Verifica se o indicador de erro está ativo no fluxo.
 * @fp:    Ponteiro para a estrutura FILE a ser consultada.
 * 
 * Retorna: Um valor diferente de zero se ocorreu um erro de E/S, ou 0 caso contrário.
 */
int ferror(FILE *fp)
{
    // Se o ponteiro for inválido, assume que não há erro ativo (retorna falso)
    if (!fp) {
        return 0;
    }

    // Avalia o bit através da máscara _IOERR que definiste no teu <stdio.h>
    // Retorna 1 se o bit estiver ligado (indicando erro), ou 0 se estiver limpo.
    return (fp->flags & _IOERR) ? 1 : 0;
}