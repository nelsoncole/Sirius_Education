/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vsnprintf.c
 *    Description: Implementação do formatador de buffers em memória (vsnprintf)
 *                 com suporte a alinhamento, preenchimento com zeros e
 *                 modificadores de tamanho de 64-bits para Ring 3.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

/* Definições de booleanos locais para conformidade semântica */
#define true  1
#define false 0

/* Tabela de caracteres hexadecimais estáticos */
static const char g_hex_chars_lower[] = "0123456789abcdef";
static const char g_hex_chars_upper[] = "0123456789ABCDEF";

/**
 * int64_to_str - Converte um inteiro de 64 bits com sinal para string.
 */
static void int64_to_str(int64_t value, char *buf) 
{
    char scratch[32];
    int idx = 0;
    int is_negative = false;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    if (value < 0) {
        is_negative = true;
        value = -value;
    }

    while (value > 0) {
        scratch[idx++] = '0' + (value % 10);
        value /= 10;
    }

    int out_idx = 0;
    if (is_negative) {
        buf[out_idx++] = '-';
    }

    for (int i = idx - 1; i >= 0; i--) {
        buf[out_idx++] = scratch[i];
    }
    buf[out_idx] = '\0';
}

/**
 * uint64_to_str - Converte um inteiro de 64 bits sem sinal para string.
 */
static void uint64_to_str(uint64_t value, char *buf) 
{
    char scratch[32];
    int idx = 0;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (value > 0) {
        scratch[idx++] = '0' + (value % 10);
        value /= 10;
    }

    int out_idx = 0;
    for (int i = idx - 1; i >= 0; i--) {
        buf[out_idx++] = scratch[i];
    }
    buf[out_idx] = '\0';
}

/**
 * hex_to_str_variable - Converte um valor numérico para representação hexadecimal.
 */
static void hex_to_str_variable(uint64_t value, char *buf, int uppercase) 
{
    char scratch[32];
    int idx = 0;
    const char *hex_table = uppercase ? g_hex_chars_upper : g_hex_chars_lower;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (value > 0) {
        scratch[idx++] = hex_table[value % 16];
        value /= 16;
    }

    int out_idx = 0;
    for (int i = idx - 1; i >= 0; i--) {
        buf[out_idx++] = scratch[i];
    }
    buf[out_idx] = '\0';
}

/**
 * vsnprintf - Formata uma string baseada em argumentos e guarda-a num buffer.
 */
int vsnprintf(char *buf, size_t max_len, const char *format, va_list ap)
{
    if (!buf || max_len == 0)
        return 0;

    size_t dest_idx = 0;
    size_t i = 0;
    char temp_buffer[128];

// Macro interna para escrever no buffer destino respeitando o limite físico
#define APPEND_CHAR(c)              \
    do                              \
    {                               \
        if (dest_idx < max_len - 1) \
        {                           \
            buf[dest_idx++] = (c);  \
        }                           \
    } while (0)

    while (format[i])
    {
        if (format[i] != '%')
        {
            APPEND_CHAR(format[i]);
            i++;
            continue;
        }

        i++; // Pula '%'

        if (format[i] == '%')
        {
            APPEND_CHAR('%');
            i++;
            continue;
        }

        // Captura o modificador de alinhamento à esquerda '-'
        int left_align = false;
        if (format[i] == '-')
        {
            left_align = true;
            i++;
        }

        size_t width = 0;
        int zero_pad = false;

        if (format[i] == '0')
        {
            zero_pad = true;
            i++;
        }

        while (format[i] >= '0' && format[i] <= '9')
        {
            width = width * 10 + (format[i] - '0');
            i++;
        }

        int has_precision = false;
        size_t precision = 0;

        if (format[i] == '.')
        {
            has_precision = true;
            i++;
            while (format[i] >= '0' && format[i] <= '9')
            {
                precision = precision * 10 + (format[i] - '0');
                i++;
            }
        }

        int long_flag = false;
        int long_long_flag = false;

        if (format[i] == 'l')
        {
            long_flag = true;
            i++;
            if (format[i] == 'l')
            {
                long_long_flag = true;
                i++;
            }
        }

        switch (format[i])
        {
        case 'c':
        {
            char c = (char)va_arg(ap, int);
            APPEND_CHAR(c);
            break;
        }

        case 's':
        {
            char *s = va_arg(ap, char *);
            if (!s)
                s = "(null)";

            // Calcula o comprimento real respeitando a precisão opcional
            size_t len = 0;
            while (s[len])
            {
                if (has_precision && len >= precision)
                    break;
                len++;
            }

            // Se alinhado à direita (padrão), insere espaços ANTES da string
            if (!left_align && width > len)
            {
                size_t pad = width - len;
                while (pad--)
                    APPEND_CHAR(' ');
            }

            // Copia a string caractere a caractere
            for (size_t k = 0; k < len; k++)
            {
                APPEND_CHAR(s[k]);
            }

            // Se alinhado à esquerda, insere os espaços DEPOIS da string
            if (left_align && width > len)
            {
                size_t pad = width - len;
                while (pad--)
                    APPEND_CHAR(' ');
            }
            break;
        }

        case 'd':
        case 'i':
        {
            int64_t value = (long_flag || long_long_flag) ? va_arg(ap, int64_t) : va_arg(ap, int32_t);
            int64_to_str(value, temp_buffer);

            size_t len = 0;
            while (temp_buffer[len])
                len++;

            // Alinhamento à direita
            if (!left_align && width > len)
            {
                size_t pad = width - len;
                while (pad--)
                    APPEND_CHAR(zero_pad ? '0' : ' ');
            }

            for (size_t k = 0; k < len; k++)
                APPEND_CHAR(temp_buffer[k]);

            // Alinhamento à esquerda
            if (left_align && width > len)
            {
                size_t pad = width - len;
                while (pad--)
                    APPEND_CHAR(' ');
            }
            break;
        }

        case 'u':
        {
            uint64_t value = (long_flag || long_long_flag) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
            uint64_to_str(value, temp_buffer);

            size_t len = 0;
            while (temp_buffer[len])
                len++;

            // Alinhamento à direita
            if (!left_align && width > len)
            {
                size_t pad = width - len;
                while (pad--)
                    APPEND_CHAR(zero_pad ? '0' : ' ');
            }

            for (size_t k = 0; k < len; k++)
                APPEND_CHAR(temp_buffer[k]);

            // Alinhamento à esquerda
            if (left_align && width > len)
            {
                size_t pad = width - len;
                while (pad--)
                    APPEND_CHAR(' ');
            }
            break;
        }

        case 'x':
        case 'X':
        {
            uint64_t value = (long_flag || long_long_flag) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
            hex_to_str_variable(value, temp_buffer, (format[i] == 'X'));

            size_t len = 0;
            while (temp_buffer[len])
                len++;

            // Alinhamento à direita
            if (!left_align && width > len)
            {
                size_t pad = width - len;
                while (pad--)
                    APPEND_CHAR(zero_pad ? '0' : ' ');
            }

            for (size_t k = 0; k < len; k++)
                APPEND_CHAR(temp_buffer[k]);

            // Alinhamento à esquerda
            if (left_align && width > len)
            {
                size_t pad = width - len;
                while (pad--)
                    APPEND_CHAR(' ');
            }
            break;
        }

        case 'p':
        {
            uintptr_t ptr = (uintptr_t)va_arg(ap, void *);
            APPEND_CHAR('0');
            APPEND_CHAR('x');

            // Processamento nativo de 64 bits (16 caracteres hexadecimais alinhados)
            hex_to_str_variable((uint64_t)ptr, temp_buffer, 0);

            size_t len = 0;
            while (temp_buffer[len])
                len++;

            // Preenchimento com zeros à esquerda para ponteiros com largura definida
            if (width > (len + 2) && !left_align)
            {
                size_t pad = width - len - 2;
                while (pad--)
                    APPEND_CHAR('0');
            }

            for (size_t k = 0; k < len; k++)
                APPEND_CHAR(temp_buffer[k]);

            if (width > (len + 2) && left_align)
            {
                size_t pad = width - len - 2;
                while (pad--)
                    APPEND_CHAR(' ');
            }
            break;
        }

        default:
            // Fallback para caracteres de formato desconhecidos
            APPEND_CHAR(format[i]);
            break;
        }

        i++; // Avança para o próximo caractere do formato
    }

    buf[dest_idx] = '\0'; // Finaliza a string de forma segura
    return (int)dest_idx;

#undef APPEND_CHAR
}