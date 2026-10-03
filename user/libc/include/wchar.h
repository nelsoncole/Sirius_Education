/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: wchar.h
 *    Description: Cabeçalho de suporte a caracteres largos (Wide Characters)
 *                 para a LibC (Ring 3). Fornece stubs de conformidade POSIX 
 *                 para permitir o build de bibliotecas portadas (como TLSe).
 * 
 *         Author:  Nelson Cole
 *   Created Date: 01/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _WCHAR_H
#define _WCHAR_H

#include <stdint.h>
#include <stddef.h>

#ifndef _WCHAR_T_DEFINED
#define _WCHAR_T_DEFINED
/* No x86_64 GCC freestanding, wchar_t é convencionalmente um int de 32 bits */
typedef int wchar_t;
#endif

#ifndef _WINT_T_DEFINED
#define _WINT_T_DEFINED
typedef unsigned int wint_t;
#endif

#ifndef WEOF
#define WEOF ((wint_t)-1)
#endif

/* Protótipos das funções elementares de strings largas exigidas em Ring 3 */
size_t  wcslen(const wchar_t *s);
wchar_t *wcscpy(wchar_t *dest, const wchar_t *src);
int     wcscmp(const wchar_t *s1, const wchar_t *s2);

/* Funções de conversão básicas para compatibilidade de amálgama */
size_t  wcstombs(char *dest, const wchar_t *src, size_t n);
size_t  mbstowcs(wchar_t *dest, const char *src, size_t n);

#endif /* _WCHAR_H */
