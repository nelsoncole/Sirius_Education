/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfprintf.c
 *    Description: Implementação do formatador baseado em va_list (vfprintf)
 *                 direcionado para fluxos genéricos (FILE *).
 *                 Reutiliza a vsnprintf para formatação segura e atómica.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 25/09/2026
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
 * vfprintf - Formata e escreve uma lista de argumentos variados num fluxo (FILE *).
 * @fp:     Ponteiro para a estrutura FILE que encapsula o File Descriptor.
 * @format: String contendo as diretivas de formatação (%d, %s, etc.).
 * @arg:    Lista de argumentos já inicializada por va_start.
 * @return: O número de caracteres impressos, ou -1 em caso de erro.
 */
int vfprintf(FILE *fp, const char *format, va_list arg)
{
    // Validação de sanidade do ponteiro de fluxo
    if (fp == NULL)
    {
        return -1;
    }

    // Buffer temporário na Stack de Ring 3 para montagem atómica do texto
    char output_buffer[1024];

    // 1. Executa a formatação completa utilizando o vosso motor vsnprintf
    int chars_formatted = vsnprintf(output_buffer, sizeof(output_buffer), format, arg);

    if (chars_formatted < 0)
    {
        return -1;
    }

    // Trunca a escrita caso a formatação tenha excedido a capacidade estável da Stack
    size_t bytes_to_write = (size_t)chars_formatted;
    if (bytes_to_write >= sizeof(output_buffer))
    {
        bytes_to_write = sizeof(output_buffer) - 1;
    }

    // 2. Transmissão para o VFS usando o descritor correspondente do fluxo (fp->fd)
    ssize_t bytes_written = write(fp->fd, output_buffer, bytes_to_write);

    return (int)bytes_written;
}