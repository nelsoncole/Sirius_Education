/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vsprintf.c
 *    Description: Implementação do formatador de strings em memória (vsprintf).
 *                 Reutiliza a lógica segura de vsnprintf passando o limite
 *                 máximo do tamanho de memória para compatibilidade POSIX.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>

/* Protótipo interno da tua vsnprintf alocada no outro ficheiro */
extern int vsnprintf(char *buf, size_t max_len, const char *format, va_list ap);

/**
 * vsprintf - Formata uma string baseada em argumentos e guarda-a num buffer.
 *            Nota: Não possui limite estrito de tamanho (perigoso se houver overflow),
 *            por isso encapsula a vsnprintf com o maior tamanho possível ((size_t)-1).
 * @buf:    Buffer de destino na memória de Ring 3.
 * @format: String de formato com os modificadores (%d, %s, etc.).
 * @ap:     Lista de argumentos variáveis (va_list).
 * @return: O número de caracteres efetivamente escritos no buffer.
 */
int vsprintf(char *buf, const char *format, va_list arg) 
{
    // (size_t)-1 resulta em 0xFFFFFFFFFFFFFFFF (o maior valor em 64-bits)
    return vsnprintf(buf, (size_t)-1, format, arg);
}