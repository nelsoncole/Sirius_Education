/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: putchar.c
 *    Description: Escreve um único caractere no fluxo de saída padrão (stdout).
 *                 Utiliza o descritor encapsulado na estrutura FILE padrão.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <unistd.h>

/**
 * putchar - Envia um único caractere para a saída padrão (stdout).
 * @c: O caractere a ser escrito (passado como int conforme o padrão ISO C).
 * @return: O próprio caractere escrito convertido para unsigned char, 
 *          oú EOF em caso de falha na chamada de sistema.
 */
int putchar(int c) 
{
    char ch = (char)c;

    // Dispara a syscall write de forma inline usando o FD do objeto global stdout
    if (write(stdout->fd, &ch, 1) == 1) 
    {
        return (unsigned char)c;
    }

    return EOF;
}