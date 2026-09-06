/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kprintf.c
 *    Description: Implementação da função de formatação e exibição de texto
 *                 no terminal do kernel (kprintf). Suporta argumentos variáveis,
 *                 modificadores de largura (width), preenchimento com zeros e
 *                 limitação de precisão de strings (ex: %.6s).
 * 
 *         Author: Nelson Cole
 *   Created Date: 29/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 31/08/2026
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
// Variável global de controlo (0 = Liberado, 1 = Ocupado)
volatile uint64_t kprintf_spinlock = 0;

// Função auxiliar para enviar strings completas ao hardware com limite opcional
static void kernel_putstring_limit(const char *str, size_t limit, int has_limit) {
    size_t count = 0;
    while (*str) {
        if (has_limit && count >= limit) {
            break;
        }
        kernel_putchar(*str);
        if (*str == '\n') {
            kernel_putchar('\r');
        }
        str++;
        count++;
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
// CONVERSÃO DE HEXADECIMAL VARIÁVEL PARA STRING
//--------------------------------------------------
static void hex_to_str_variable(uint64_t value, char *buffer, int uppercase) {
    char temp[32];
    size_t i = 0;

    do {
        size_t nibble = value & 0xF;
        if (nibble < 10) {
            temp[i++] = '0' + nibble;
        } else {
            temp[i++] = (uppercase ? 'A' : 'a') + nibble - 10;
        }
        value >>= 4;
    } while (value);

    size_t j = 0;
    while (i) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

//--------------------------------------------------
// KPRINTF CORE A 100%
//--------------------------------------------------
int bootverbose;
void kprintf(const char *format, ...) {
    if(bootverbose) return;
    char buffer[128];
    va_list ap;

    // ============================================================================
    // SPINLOCK OBRIGATÓRIO PARA MULTIPROCESSAMENTO (SMP)
    // Bloqueia a entrada se outra CPU já estiver a usar o hardware de vídeo
    // ============================================================================
    while (__atomic_test_and_set(&kprintf_spinlock, __ATOMIC_ACQUIRE)) {
        __asm__ __volatile__("pause" ::: "memory");
    }

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

        // NOVO: Modificador de Precisão/Limite (ponto '.') ex: %.6s
        int has_precision = false;
        size_t precision = 0;

        if (format[i] == '.') {
            has_precision = true;
            i++;
            while (format[i] >= '0' && format[i] <= '9') {
                precision = precision * 10 + (format[i] - '0');
                i++;
            }
        }

        // Modificadores de tamanho de dados: l / ll
        int long_flag = false;
        int long_long_flag = false;

        if (format[i] == 'l') {
            long_flag = true;
            i++;
            if (format[i] == 'l') {
                long_long_flag = true;
                i++;
            }
        }

        switch (format[i]) {
            // Caractere
            case 'c': {
                char c = (char)va_arg(ap, int);
                kernel_putchar(c);
                break;
            }

            // String (Com suporte ao modificador de precisão dinâmico)
            case 's': {
                char *s = va_arg(ap, char *);
                if (!s) s = "(null)";
                
                kernel_putstring_limit(s, precision, has_precision);
                break;
            }

            // Inteiros Sinalizados
            case 'd':
            case 'i': {
                int64_t value = (long_flag || long_long_flag) ? va_arg(ap, int64_t) : va_arg(ap, int32_t);
                int64_to_str(value, buffer);

                size_t len = 0;
                while (buffer[len]) len++;
                if (width > len) {
                    size_t pad = width - len;
                    while (pad--) kernel_putchar(zero_pad ? '0' : ' ');
                }
                kernel_putstring_limit(buffer, 0, false);
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
                kernel_putstring_limit(buffer, 0, false);
                break;
            }

            // Hexadecimais Dinâmicos Otimizados
            case 'x':
            case 'X': {
                uint64_t value = (long_flag || long_long_flag) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
                
                // Converte de forma natural e compacta
                hex_to_str_variable(value, buffer, (format[i] == 'X'));

                size_t len = 0;
                while (buffer[len]) len++;
                
                // Aplica o preenchimento com zeros (ex: %08x) ou espaços com base no width
                if (width > len) {
                    size_t pad = width - len;
                    while (pad--) kernel_putchar(zero_pad ? '0' : ' ');
                }
                
                kernel_putstring_limit(buffer, 0, false);
                break;
            }

            // Ponteiros (Garante a impressão de 16 caracteres em x86_64)
            case 'p': {
                uintptr_t ptr = (uintptr_t)va_arg(ap, void *);
                kernel_putstring_limit("0x", 0, false);
                
                // Ponteiro em 64 bits sempre imprime o bloco completo de 16 caracteres hexadecimais
                char temp_hex[16];
                for (int idx = 0; idx < 16; idx++) {
                    size_t nibble = (ptr >> ((15 - idx) * 4)) & 0xF;
                    if (nibble < 10) {
                        temp_hex[idx] = '0' + nibble;
                    } else {
                        temp_hex[idx] = 'a' + nibble - 10;
                    }
                }
                kernel_putstring_limit(temp_hex, 16, true);
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

    // ============================================================================
    // LIBERTAÇÃO DO TRINCO
    // Permite que o próximo núcleo da fila possa usar o kprintf
    // ============================================================================
    __atomic_clear(&kprintf_spinlock, __ATOMIC_RELEASE);
}
