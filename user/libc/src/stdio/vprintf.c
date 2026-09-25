/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vprintf.c
 *    Description: Implementação do formatador baseado em va_list (vprintf)
 *                 direcionado para o fluxo de saída padrão (stdout).
 *                 Reutiliza a vsnprintf para formatação segura e atómica.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <stddef.h>

/* Protótipo da fábrica de formatação que já implementaste */
extern int vsnprintf(char *buf, size_t max_len, const char *format, va_list ap);

/**
 * vprintf - Formata e escreve uma lista de argumentos variados na saída padrão.
 * @format: String contendo as diretivas de formatação (%d, %s, etc.).
 * @arg:    Lista de argumentos já inicializada por va_start.
 * @return: O número de caracteres impressos no ecrã, ou -1 em caso de erro grave.
 */
int vprintf(const char *format, va_list arg)
{
    // Buffer temporário na Stack de Ring 3 para montagem atómica do texto.
    // 1024 bytes é o padrão POSIX seguro para uma linha de terminal.
    char output_buffer[1024];

    // 1. Executa a formatação completa com suporte a 64-bits e alinhamentos
    int chars_formatted = vsnprintf(output_buffer, sizeof(output_buffer), format, arg);

    if (chars_formatted < 0)
    {
        return -1;
    }

    // 2. Transmissão atómica para o VFS
    // Usa o File Descriptor encapsulado na estrutura global stdout (que aponta para STDOUT_FILENO)
    ssize_t bytes_written = write(stdout->fd, output_buffer, (size_t)chars_formatted);

    return (int)bytes_written;
}