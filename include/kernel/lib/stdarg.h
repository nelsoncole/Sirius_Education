/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: stdarg.h
 *    Description: Declarações e macros para manipulação de listas de 
 *                 argumentos variáveis (funções variádicas) para o core do kernel.
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
#ifndef _STDARG_H_
#define _STDARG_H_

/*
 * Compatível com:
 * i386
 * x86_64
 */

typedef __builtin_va_list va_list;

/*
 * Inicializa a lista de argumentos
 */
#define va_start(ap, last) \
    __builtin_va_start(ap, last)

/*
 * Obtém o próximo argumento
 */
#define va_arg(ap, type) \
    __builtin_va_arg(ap, type)

/*
 * Finaliza a lista
 */
#define va_end(ap) \
    __builtin_va_end(ap)

/*
 * Copia uma lista de argumentos
 */
#define va_copy(dest, src) \
    __builtin_va_copy(dest, src)

#endif