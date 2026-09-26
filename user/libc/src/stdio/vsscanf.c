/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vsscanf.c
 *    Description: Implementação do motor de parsing baseado em strings e
 *                 va_list (vsscanf) complementar à sua vsnprintf.
 *                 Suporta larguras de campo e modificadores de 64-bits.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 25/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

/**
 * parse_integer_vsscanf - Converte uma substring para inteiro de 64-bits.
 */
static int64_t parse_integer_vsscanf(const char *buf, int base) {
    int64_t result = 0;
    int negative = 0;

    while (isspace((unsigned char)*buf)) buf++;

    if (*buf == '-') {
        negative = 1;
        buf++;
    } else if (*buf == '+') {
        buf++;
    }

    while (*buf) {
        char ch = *buf;
        int digit;

        if (ch >= '0' && ch <= '9') digit = ch - '0';
        else if (ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;
        else break;

        if (digit >= base) break;

        result = result * base + digit;
        buf++;
    }

    return negative ? -result : result;
}

/**
 * vsscanf - Faz o parsing de uma string com base num formato e numa va_list.
 */
int vsscanf(const char *str, const char *fmt, va_list ap) {
    int assigned = 0;
    const char *p_str = str;
    const char *p_fmt = fmt;

    while (*p_fmt) {

        // 1. Espaço no formato descarta qualquer espaço correspondente na string
        if (isspace((unsigned char)*p_fmt)) {
            while (isspace((unsigned char)*p_fmt)) p_fmt++;
            while (isspace((unsigned char)*p_str)) p_str++;
            continue;
        }

        // 2. Correspondência de caracteres literais
        if (*p_fmt != '%') {
            if (*p_fmt != *p_str) break;
            p_fmt++; p_str++;
            continue;
        }

        p_fmt++; // Salta '%'

        // Trata o literal '%%'
        if (*p_fmt == '%') {
            if (*p_str != '%') break;
            p_fmt++; p_str++;
            continue;
        }

        // Captura a flag de supressão de atribuição '*'
        int ignore = 0;
        if (*p_fmt == '*') {
            ignore = 1;
            p_fmt++;
        }

        // Captura a largura máxima do campo (ex: %10s)
        int width = 0;
        while (isdigit((unsigned char)*p_fmt)) {
            width = width * 10 + (*p_fmt - '0');
            p_fmt++;
        }

        // Captura os modificadores de tamanho 'l' ou 'll' (coerente com a sua vsnprintf)
        int long_flag = 0;
        int long_long_flag = 0;

        if (*p_fmt == 'l') {
            long_flag = 1;
            p_fmt++;
            if (*p_fmt == 'l') {
                long_long_flag = 1;
                p_fmt++;
            }
        }

        char spec = *p_fmt++;
        if (!spec) break;

        // Quase todos os especificadores (exceto %c) ignoram espaços brancos iniciais
        if (spec != 'c') {
            while (isspace((unsigned char)*p_str)) p_str++;
        }

        // ====================================================================
        // PROCESSAMENTO DOS ESPECIFICADORES DE FORMATO
        // ====================================================================

        if (spec == 'd' || spec == 'i') {
            char num_buf[64] = {0};
            int i = 0;
            int max_chars = (width && width < 63) ? width : 63;

            if (*p_str == '-' || *p_str == '+') {
                num_buf[i++] = *p_str++;
            }

            while (isdigit((unsigned char)*p_str) && i < max_chars) {
                num_buf[i++] = *p_str++;
            }

            if (i == 0 || (i == 1 && (num_buf[0] == '-' || num_buf[0] == '+'))) break;

            if (!ignore) {
                int64_t val = parse_integer_vsscanf(num_buf, 10);
                if (long_flag || long_long_flag) {
                    int64_t *out = va_arg(ap, int64_t *);
                    *out = val;
                } else {
                    int32_t *out = va_arg(ap, int32_t *);
                    *out = (int32_t)val;
                }
                assigned++;
            }
        }
        else if (spec == 'u' || spec == 'x' || spec == 'X') {
            char num_buf[64] = {0};
            int i = 0;
            int max_chars = (width && width < 63) ? width : 63;
            int base = (spec == 'u') ? 10 : 16;

            if (base == 16 && *p_str == '0' && (*(p_str + 1) == 'x' || *(p_str + 1) == 'X')) {
                p_str += 2; // Ignora o prefixo hexadecimal 0x
            }

            while ((base == 10 ? isdigit((unsigned char)*p_str) : isxdigit((unsigned char)*p_str)) && i < max_chars) {
                num_buf[i++] = *p_str++;
            }

            if (i == 0) break;

            if (!ignore) {
                uint64_t val = (uint64_t)parse_integer_vsscanf(num_buf, base);
                if (long_flag || long_long_flag) {
                    uint64_t *out = va_arg(ap, uint64_t *);
                    *out = val;
                } else {
                    uint32_t *out = va_arg(ap, uint32_t *);
                    *out = (uint32_t)val;
                }
                assigned++;
            }
        }
        else if (spec == 's') {
            int count = 0;
            const char *start = p_str;

            while (*p_str && !isspace((unsigned char)*p_str) && (!width || count < width)) {
                p_str++;
                count++;
            }

            if (count == 0) break;

            if (!ignore) {
                char *out = va_arg(ap, char *);
                memcpy(out, start, count);
                out[count] = '\0';
                assigned++;
            }
        }
        else if (spec == 'c') {
            int n = width ? width : 1;
            if (!*p_str) break;

            if (!ignore) {
                char *out = va_arg(ap, char *);
                for (int i = 0; i < n && *p_str; i++) {
                    *out++ = *p_str++;
                }
                assigned++;
            } else {
                for (int i = 0; i < n && *p_str; i++) p_str++;
            }
        }
        else {
            break; // Especificador inválido/não suportado
        }
    }

    return assigned;
}