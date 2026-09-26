/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sscanf.c
 *    Description: Implementação da função padrão sscanf() da libc.
 *                 Atua como um wrapper em Ring 3 para a vsscanf().
 * 
 *        Author:  Nelson Cole
 *   Created Date: 25/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <stdarg.h>

/**
 * sscanf - Lê dados formatados a partir de uma string em memória.
 * @str:    A string de origem que contém os dados a serem processados.
 * @fmt:    A string de template com as diretivas de formatação (%d, %s, etc.).
 * @...:    Ponteiros para as variáveis onde os dados extraídos serão guardados.
 * @return: O número de campos preenchidos com sucesso, ou EOF em caso de falha imediata.
 */
int sscanf(const char *str, const char *fmt, ...)
{
    va_list args;
    int fields_assigned;

    // 1. Inicializa a lista de argumentos variados após o parâmetro 'fmt'
    va_start(args, fmt);

    // 2. Delega o parsing para o motor atómico vsscanf
    fields_assigned = vsscanf(str, fmt, args);

    // 3. Liberta a stack de argumentos de forma limpa
    va_end(args);

    return fields_assigned;
}