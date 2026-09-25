/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: fputs.c
 *    Description: Escreve uma string num fluxo de arquivo (stream) específico,
 *                 sem anexar uma quebra de linha (\n) automática.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>

/**
 * fputs - Escreve uma string no fluxo de saída especificado.
 * @s:      Ponteiro para a string terminada em nulo.
 * @stream: Ponteiro para a estrutura FILE que identifica o fluxo (ex: stdout).
 * @return: Um valor não-negativo em caso de sucesso, ou EOF se falhar.
 */
int fputs(const char *s, FILE *stream)
{
    if (!s || !stream)
    {
        return EOF;
    }

    // 1. Calcula o comprimento real da string de texto
    size_t len = strlen(s);

    // 2. Se a string estiver vazia, POSIX considera sucesso imediato
    if (len == 0)
    {
        return 0;
    }

    // 3. Transmite os bytes brutos para o File Descriptor contido na stream
    // Se a stream for 'stdout', o Kernel redireciona para a tua PTY ou TTY0
    if (write(stream->fd, s, len) != (ssize_t)len)
    {
        return EOF;
    }

    // Retorna 0 (valor não-negativo) para indicar conformidade com o padrão
    return 0;
}