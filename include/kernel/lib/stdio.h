/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: stdio.h
 *    Description: Protótipos das funções de entrada e saída padrão do kernel.
 * 
 *         Author: Nelson Cole
 *   Created Date: 29/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 29/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _STDIO_H_
#define _STDIO_H_

#include <kernel/lib/stdarg.h>
#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>
#include <kernel/lib/stdbool.h>

int kvsnprintf(char *buf, size_t max_len, const char *format, va_list ap);

int ksprintf(char *buf, const char *format, ...);

/*
 * Função principal de formatação e exibição de texto no terminal do kernel.
 */
void kprintf(const char *format, ...);

#endif /* _STDIO_H_ */
