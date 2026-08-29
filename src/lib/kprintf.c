/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kprintf.c
 *    Description: Implementação da função de formatação e exibição de texto
 *                 no terminal do kernel (kprintf). Suporta argumentos variáveis.
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

#include <kernel/lib/stdarg.h>
#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>
#include <kernel/lib/stdbool.h>
#include <kernel/lib/stdio.h>

// Prototipagem da sua função de baixo nível do terminal do kernel
extern void kernel_putchar(char c);

// Função auxiliar para enviar strings completas ao hardware
static void kernel_putstring(const char *str) {
    while (*str) {
        kernel_putchar(*str);
        if (*str == '\n') {
            kernel_putchar('\r');
        }
        str++;
    }
}

//--------------------------------------------------
// CONVERSÃO DE INTEIRO NÃO SINALIZADO PARA STRING
//--------------------------------------------------
static void uint64_to_str(uint64_t value, char *buffer) {
    char temp[32];
    size_t i = 0;

    do {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    } while (value);

    size_t j = 0;
    while (i) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

//--------------------------------------------------
// CONVERSÃO DE INTEIRO SINALIZADO PARA STRING
//--------------------------------------------------
static void int64_to_str(int64_t value, char *buffer) {
    uint64_t n;
    char temp[32];
    size_t i = 0;
    int negative = false;

    if (value < 0) {
        negative = true;
        n = (uint64_t)(-(value + 1)) + 1;
    } else {
        n = value;
    }

    do {
        temp[i++] = '0' + (n % 10);
        n /= 10;
    } while (n);

    if (negative) {
        temp[i++] = '-';
    }

    size_t j = 0;
    while (i) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

//--------------------------------------------------
// CONVERSÃO DE HEXADECIMAL PARA STRING (COM COMPRIMENTO)
//--------------------------------------------------
static void hex_to_str(uint64_t value, char *buffer, size_t digits, int uppercase) {
    for (size_t i = 0; i < digits; i++) {
        size_t nibble = (value >> ((digits - i - 1) * 4)) & 0xF;
        if (nibble < 10) {
            buffer[i] = '0' + nibble;
        } else {
            buffer[i] = (uppercase ? 'A' : 'a') + nibble - 10;
        }
    }
    buffer[digits] = '\0';
}

//--------------------------------------------------
// KPRINTF CORE
//--------------------------------------------------
void kprintf(const char *format, ...) {
    char buffer[128];
    va_list ap;

    va_start(ap, format);

    size_t i = 0;
    while (format[i]) {
        if (format[i] != '%') {
            kernel_putchar(format[i]);
            if (format[i] == '\n') {
                kernel_putchar('\r');
            }
            i++;
            continue;
        }

        i++; // Pula o '%'

        if (format[i] == '%') {
            kernel_putchar('%');
            i++;
            continue;
        }

        int long_flag = false;
        int long_long_flag = false;

        // Modificadores de tamanho: l / ll
        if (format[i] == 'l') {
            long_flag = true;
            i++;
            if (format[i] == 'l') {
                long_long_flag = true;
                i++;
            }
        }

        // Modificadores de Largura (Width) e Padding com zeros
        size_t width = 0;
        int zero_pad = false;

        if (format[i] == '0') {
            zero_pad = true;
            i++;
        }

        while (format[i] >= '0' && format[i] <= '9') {
            width = width * 10 + (format[i] - '0');
            i++;
        }

        switch (format[i]) {
            // Caractere
            case 'c': {
                char c = (char)va_arg(ap, int);
                kernel_putchar(c);
                break;
            }

            // String
            case 's': {
                char *s = va_arg(ap, char *);
                if (!s) s = "(null)";
                kernel_putstring(s);
                break;
            }

            // Inteiros Sinalizados
            case 'd':
            case 'i': {
                int64_t value = (long_flag || long_long_flag) ? va_arg(ap, int64_t) : va_arg(ap, int32_t);
                int64_to_str(value, buffer);

                // Aplica padding se necessário
                size_t len = 0;
                while (buffer[len]) len++;
                if (width > len) {
                    size_t pad = width - len;
                    while (pad--) kernel_putchar(zero_pad ? '0' : ' ');
                }
                kernel_putstring(buffer);
                break;
            }

            // Inteiros Não Sinalizados
            case 'u': {
                uint64_t value = (long_flag || long_long_flag) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
                uint64_to_str(value, buffer);

                size_t len = 0;
                while (buffer[len]) len++;
                if (width > len) {
                    size_t pad = width - len;
                    while (pad--) kernel_putchar(zero_pad ? '0' : ' ');
                }
                kernel_putstring(buffer);
                break;
            }

            // Hexadecimais
            case 'x':
            case 'X': {
                uint64_t value = (long_flag || long_long_flag) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
                size_t digits = width ? width : ((long_flag || long_long_flag) ? 16 : 8);

                hex_to_str(value, buffer, digits, (format[i] == 'X'));
                kernel_putstring(buffer);
                break;
            }

            // Ponteiros
            case 'p': {
                uintptr_t ptr = (uintptr_t)va_arg(ap, void *);
                kernel_putstring("0x");
                hex_to_str(ptr, buffer, sizeof(uintptr_t) * 2, false);
                kernel_putstring(buffer);
                break;
            }

            // Especificador desconhecido
            default: {
                kernel_putchar('%');
                kernel_putchar(format[i]);
                break;
            }
        }
        i++;
    }

    va_end(ap);
}
