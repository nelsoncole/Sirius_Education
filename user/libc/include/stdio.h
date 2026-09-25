/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: stdio.h
 *    Description: Cabeçalho padrão de Entrada/Saída (Standard I/O) para Ring 3.
 *                 Em conformidade com o padrão ISO C e POSIX.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * ============================================================================
 */

#ifndef _STDIO_H
#define _STDIO_H

#include <sys/types.h>
#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)

/* ISO C: Estrutura que encapsula o File Descriptor */
typedef struct {
    int fd;
    int flags;
} FILE;

/* POSIX / ISO C: Fluxos padrão globais que as funções do C usam */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Funções Principais de Formatação e Parsing */
int printf(const char *format, ...);
int vprintf(const char *format, va_list arg);
int sprintf(char *str, const char *format, ...);
int vsprintf(char *str, const char *format, va_list arg);
int vsnprintf(char *buf, size_t max_len, const char *format, va_list ap);

/* Funções de Caracteres e Strings Base de stdio */
int putchar(int c);
int puts(const char *s);
int fputs(const char *s, FILE *stream);

#endif /* _STDIO_H */