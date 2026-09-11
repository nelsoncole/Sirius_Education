/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kprintf.c
 *    Description: Implementação das funções de formatação e exibição de texto
 *                 (kprintf, ksprintf e kvsnprintf). Suporta argumentos variáveis,
 *                 modificadores de largura (width), preenchimento com zeros,
 *                 alinhamento à esquerda (ex: %-12s) e limitação de precisão.
 * 
 *         Author: Nelson Cole
 *   Created Date: 29/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 11/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/lib/stdio.h>

extern void kernel_putchar(char c);
volatile uint64_t kprintf_spinlock = 0;
int bootverbose = 0;

/* --- Funções Auxiliares de Conversão --- */

static void uint64_to_str(uint64_t value, char *buffer) {
    char temp[32];
    size_t i = 0;
    do {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    } while (value);

    size_t j = 0;
    while (i) buffer[j++] = temp[--i];
    buffer[j] = '\0';
}

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

    if (negative) temp[i++] = '-';

    size_t j = 0;
    while (i) buffer[j++] = temp[--i];
    buffer[j] = '\0';
}

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
    while (i) buffer[j++] = temp[--i];
    buffer[j] = '\0';
}

//-----------------------------------------------------------------------------
// O MOTOR CORE: KVSNPRINTF (Formata diretamente num buffer de memória)
//-----------------------------------------------------------------------------
int kvsnprintf(char *buf, size_t max_len, const char *format, va_list ap) {
    if (!buf || max_len == 0) return 0;

    size_t dest_idx = 0;
    size_t i = 0;
    char temp_buffer[128];

    // Macro interna para escrever no buffer destino respeitando o limite físico
    #define APPEND_CHAR(c) do { \
        if (dest_idx < max_len - 1) { \
            buf[dest_idx++] = (c); \
        } \
    } while(0)

    while (format[i]) {
        if (format[i] != '%') {
            APPEND_CHAR(format[i]);
            i++;
            continue;
        }

        i++; // Pula '%'

        if (format[i] == '%') {
            APPEND_CHAR('%');
            i++;
            continue;
        }

        // NOVO: Captura o modificador de alinhamento à esquerda '-'
        int left_align = false;
        if (format[i] == '-') {
            left_align = true;
            i++;
        }

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
            case 'c': {
                char c = (char)va_arg(ap, int);
                APPEND_CHAR(c);
                break;
            }

            case 's': {
                char *s = va_arg(ap, char *);
                if (!s) s = "(null)";
                
                // Calcula o comprimento real respeitando a precisão opcional
                size_t len = 0;
                while (s[len]) {
                    if (has_precision && len >= precision) break;
                    len++;
                }

                // Se alinhado à direita (padrão), insere espaços ANTES da string
                if (!left_align && width > len) {
                    size_t pad = width - len;
                    while (pad--) APPEND_CHAR(' ');
                }

                // Copia a string caractere a caractere
                for (size_t k = 0; k < len; k++) {
                    APPEND_CHAR(s[k]);
                }

                // Se alinhado à esquerda, insere os espaços DEPOIS da string
                if (left_align && width > len) {
                    size_t pad = width - len;
                    while (pad--) APPEND_CHAR(' ');
                }
                break;
            }

            case 'd':
            case 'i': {
                int64_t value = (long_flag || long_long_flag) ? va_arg(ap, int64_t) : va_arg(ap, int32_t);
                int64_to_str(value, temp_buffer);

                size_t len = 0;
                while (temp_buffer[len]) len++;

                // Alinhamento à direita
                if (!left_align && width > len) {
                    size_t pad = width - len;
                    while (pad--) APPEND_CHAR(zero_pad ? '0' : ' ');
                }
                
                for (size_t k = 0; k < len; k++) APPEND_CHAR(temp_buffer[k]);

                // Alinhamento à esquerda
                if (left_align && width > len) {
                    size_t pad = width - len;
                    while (pad--) APPEND_CHAR(' ');
                }
                break;
            }

            case 'u': {
                uint64_t value = (long_flag || long_long_flag) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
                uint64_to_str(value, temp_buffer);

                size_t len = 0;
                while (temp_buffer[len]) len++;

                // Alinhamento à direita
                if (!left_align && width > len) {
                    size_t pad = width - len;
                    while (pad--) APPEND_CHAR(zero_pad ? '0' : ' ');
                }
                
                for (size_t k = 0; k < len; k++) APPEND_CHAR(temp_buffer[k]);

                // Alinhamento à esquerda
                if (left_align && width > len) {
                    size_t pad = width - len;
                    while (pad--) APPEND_CHAR(' ');
                }
                break;
            }

            case 'x':
            case 'X': {
                uint64_t value = (long_flag || long_long_flag) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
                hex_to_str_variable(value, temp_buffer, (format[i] == 'X'));

                size_t len = 0;
                while (temp_buffer[len]) len++;

                // Alinhamento à direita
                if (!left_align && width > len) {
                    size_t pad = width - len;
                    while (pad--) APPEND_CHAR(zero_pad ? '0' : ' ');
                }
                
                for (size_t k = 0; k < len; k++) APPEND_CHAR(temp_buffer[k]);

                // Alinhamento à esquerda
                if (left_align && width > len) {
                    size_t pad = width - len;
                    while (pad--) APPEND_CHAR(' ');
                }
                break;
            }

            case 'p': {
                uintptr_t ptr = (uintptr_t)va_arg(ap, void *);
                APPEND_CHAR('0');
                APPEND_CHAR('x');
                
                // Processamento nativo de 64 bits (16 caracteres hexadecimais alinhados)
                for (int shift = 60; shift >= 0; shift -= 4) {
                    size_t nibble = (ptr >> shift) & 0xF;
                    if (nibble < 10) {
                        APPEND_CHAR('0' + nibble);
                    } else {
                        APPEND_CHAR('a' + nibble - 10);
                    }
                }
                break;
            }

            default:
                APPEND_CHAR(format[i]);
                break;
        }
        i++;
    }

    buf[dest_idx] = '\0';
    return (int)dest_idx;
    #undef APPEND_CHAR
}

//-----------------------------------------------------------------------------
// IMPLEMENTAÇÃO DE KSPRINTF (Exposto em stdio.h)
//-----------------------------------------------------------------------------
int ksprintf(char *buf, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int written = kvsnprintf(buf, 0x200000, format, ap);
    va_end(ap);
    return written;
}

//-----------------------------------------------------------------------------
// IMPLEMENTAÇÃO DE KPRINTF (Utiliza o Core e envia para a TTY)
//-----------------------------------------------------------------------------
void kprintf(const char *format, ...) {
    if (bootverbose) return;
    
    char write_buf[1024]; // Buffer de paginação temporário para a TTY
    va_list ap;

    while (__atomic_test_and_set(&kprintf_spinlock, __ATOMIC_ACQUIRE)) {
        __asm__ __volatile__("pause" ::: "memory");
    }

    va_start(ap, format);
    int len = kvsnprintf(write_buf, sizeof(write_buf), format, ap);
    va_end(ap);

    // Envia o bloco formatado da memória direto para o hardware de saída
    for (int i = 0; i < len; i++) {
        kernel_putchar(write_buf[i]);
        if (write_buf[i] == '\n') {
            kernel_putchar('\r');
        }
    }

    __atomic_clear(&kprintf_spinlock, __ATOMIC_RELEASE);
}
