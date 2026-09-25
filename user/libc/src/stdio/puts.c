/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: puts.c
 *    Description: Escreve uma string na saída padrão (stdout) seguida de uma
 *                 quebra de linha (\n) automática.
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
 * puts - Envia uma string de caracteres para o fluxo stdout, anexando um '\n'.
 * @s: Ponteiro para la string terminada em nulo na memória de Ring 3.
 * @return: Um valor não-negativo em caso de sucesso, ou EOF se falhar.
 */
int puts(const char *s)
{
    if (!s)
    {
        s = "(null)";
    }

    // 1. Calcula o comprimento real da string de texto
    size_t len = strlen(s);

    // 2. Transmite o corpo da string de uma só vez para o VFS
    if (len > 0)
    {
        if (write(stdout->fd, s, len) != (ssize_t)len)
        {
            return EOF;
        }
    }

    // 3. Acrescenta a quebra de linha obrigatória exigida por lei POSIX/ISO C
    char newline = '\n';
    if (write(stdout->fd, &newline, 1) != 1)
    {
        return EOF;
    }

    // O padrão POSIX dita que deve retornar um valor não-negativo em caso de sucesso
    return 0;
}