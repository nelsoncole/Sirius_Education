/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: string.h
 *    Description: Declarações das funções de manipulação de strings e blocos
 *                 de memória (libk) para o core do kernel.
 * 
 *         Author: Nelson Cole
 *   Created Date: 27/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 29/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _STRING_H_
#define _STRING_H_

#include <stddef.h>

void *memset(void *s, char val, size_t count);
void *memcpy(void * restrict s1, const void * restrict s2, size_t n);
size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
int strncpy(char *dest, const char *src,size_t count);
int memcmp(char *s1, char *s2, int n);

int strcmp (const char* s1, const char* s2);


#endif
