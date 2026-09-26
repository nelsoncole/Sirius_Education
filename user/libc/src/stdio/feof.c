#include <stdio.h>

/**
 * feof - Verifica se o indicador de fim de ficheiro (End-of-File) está ativo.
 * @fp:  Ponteiro para a estrutura FILE a ser consultada.
 * 
 * Retorna: Um valor diferente de zero se o EOF foi atingido, ou 0 caso contrário.
 */
int feof(FILE *fp)
{
    // Se o ponteiro for inválido, assume que não há EOF (retorna falso)
    if (!fp) {
        return 0;
    }

    // Avalia o bit através da máscara _IOEOF que definiste no teu <stdio.h>
    // Retorna verdadeiro (diferente de zero) se o bit estiver ligado, ou 0 se desligado.
    return (fp->flags & _IOEOF) ? 1 : 0;
}