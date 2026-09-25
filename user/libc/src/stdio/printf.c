/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: printf.c
 *    Description: Interface pública do formatador de saída padrão (printf)
 *                 para espaço de utilizador (Ring 3).
 *                 Encapsula e delega o parsing para a função vprintf.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <stdarg.h>

/**
 * printf - Escreve uma string formatada no fluxo de saída padrão (stdout).
 * @format: String com texto estático e especificadores de formato (%s, %d, etc.).
 * @...:    Argumentos variáveis correspondentes aos especificadores.
 * @return: O número total de caracteres impressos com sucesso no ecrã.
 */
int printf(const char *format, ...)
{
    va_list args;
    int bytes_written;

    // 1. Inicializa a lista de argumentos variáveis com base no parâmetro format
    va_start(args, format);

    // 2. Delega a formatação e a escrita atómica (write) para o motor do vprintf
    bytes_written = vprintf(format, args);

    // 3. Limpa a stack de argumentos de forma segura
    va_end(args);

    return bytes_written;
}