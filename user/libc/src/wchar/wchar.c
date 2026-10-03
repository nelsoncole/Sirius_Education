/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: wchar.c
 *    Description: Implementação básica de stubs de manipulação de Wide Strings
 *                 para a LibC estática (Ring 3). Focada em conformidade de build.
 * 
 *         Author:  Nelson Cole
 *   Created Date: 01/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <wchar.h>

/**
 * wcslen - Calcula o comprimento de uma string de caracteres largos.
 */
size_t wcslen(const wchar_t *s)
{
    size_t len = 0;
    while (s[len] != 0) {
        len++;
    }
    return len;
}

/**
 * wcscpy - Copia uma string de caracteres largos.
 */
wchar_t *wcscpy(wchar_t *dest, const wchar_t *src)
{
    size_t i = 0;
    while ((dest[i] = src[i]) != 0) {
        i++;
    }
    return dest;
}

/**
 * wcscmp - Compara duas strings de caracteres largos.
 */
int wcscmp(const wchar_t *s1, const wchar_t *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/**
 * wcstombs - Conversão rudimentar de Wide String para ASCII de 8 bits.
 */
size_t wcstombs(char *dest, const wchar_t *src, size_t n)
{
    size_t i = 0;
    if (!src) return (size_t)-1;

    if (!dest) {
        return wcslen(src);
    }

    for (i = 0; i < n; i++) {
        dest[i] = (char)(src[i] & 0xFF); // Trunca para ASCII 8 bits
        if (dest[i] == '\0') return i;
    }
    return i;
}

/**
 * mbstowcs - Conversão rudimentar de ASCII de 8 bits para Wide String.
 */
size_t mbstowcs(wchar_t *dest, const char *src, size_t n)
{
    size_t i = 0;
    if (!src) return (size_t)-1;

    if (!dest) {
        size_t len = 0;
        while (src[len]) len++;
        return len;
    }

    for (i = 0; i < n; i++) {
        dest[i] = (wchar_t)src[i];
        if (src[i] == '\0') return i;
    }
    return i;
}