/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfscanf.c
 *    Description: Mecanismo de parsing formatado baseado em va_list (vfscanf)
 * 
 *        Author:  Nelson Cole
 *   Created Date: 25/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

// Lê um único caractere
static int character(FILE *fp, char *dst) {
    int ch = fgetc(fp);
    if (ch == EOF) return -1;
    *dst = (char)ch;
    return 0;
}

// Lê uma string (sem espaços)
static int str(FILE *fp, char *dst) {
    int ch;
    char *s = dst;

    while ((ch = fgetc(fp)) != EOF && isspace(ch));
    if (ch == EOF) return -1;

    do {
        *s++ = ch;
        ch = fgetc(fp);
    } while (ch != EOF && !isspace(ch) && (s - dst) < 255);
    *s = '\0';

    return 0;
}

// Lê um número inteiro ou hexadecimal
static long long parse_integer(const char *buf, int base) {
    long long result = 0;
    int negative = 0;

    while (isspace(*buf)) buf++;

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

static int value(FILE *fp, void *dst, int size, int base) {
    char buf[256] = {0};
    char *p = buf;
    int ch;

    // Ignora espaços iniciais
    while ((ch = fgetc(fp)) != EOF && isspace(ch));
    if (ch == EOF) return -1;

    // Lê o número como string
    do {
        *p++ = (char)ch;
        ch = fgetc(fp);
    } while (ch != EOF && !isspace(ch) && (p - buf) < 255);
    *p = '\0';

    // Converte o número
    long long val = parse_integer(buf, base);

    // Armazena no tipo correto baseado no tamanho
    if (size == 1)
        *(char *)dst = (char)val;
    else if (size == 2)
        *(short *)dst = (short)val;
    else if (size == 4)
        *(int *)dst = (int)val;
    else if (size == 8)
        *(long long *)dst = val;
    else
        return -1;

    return 0;
}

// Lê valor de ponto flutuante
static int fvalue(FILE *fp, void *dst, int size) {
    char buf[256] = {0};
    char *p = buf;
    int ch;

    while ((ch = fgetc(fp)) != EOF && isspace(ch));
    if (ch == EOF) return -1;

    do {
        *p++ = (char)ch;
        ch = fgetc(fp);
    } while (ch != EOF && !isspace(ch) && (p - buf) < 255);
    *p = '\0';

    if (size == 8)
        *(double *)dst = atof(buf);
    else if (size == 4)
        *(float *)dst = (float)atof(buf);

    return 0;
}

// Mapeia os modificadores de formato
static int vf(char c) {
    switch (c) {
        case 'c': return 0x01;
        case 's':
        case 'S': return 0x02;
        case 'd':
        case 'i': return 0x03;
        case 'u': return 0x04;
        case 'x':
        case 'X': return 0x05;
        case 'g': return 0x06;
        case 'f': return 0x07;
        default:  return -1;
    }
}

// Função principal de parsing corrigida
int vfscanf(FILE *fp, const char *fmt, va_list ap) {
    int index = 0;
    int count = 0;

    while (fmt[index]) {
      
        if (isspace((unsigned char)fmt[index])) {
            while (isspace((unsigned char)fmt[index])) index++; 
            continue; 
        }

        // CORREÇÃO: Valida se encontrou o início de um token formatador
        if (fmt[index] != '%') {
            index++; // Ignora e passa caractere literal do template
            continue;
        }

        // Avança o '%' para inspecionar o modificador de tipo
        index++;

        // Tratamento especial para o literal '%%'
        if (fmt[index] == '%') {
            index++;
            continue;
        }

        int type = 0;
        // Deteta modificadores de tamanho de 64-bits ('l')
        if (fmt[index] == 'l') {
            type |= 0x10;
            index++;
        }

        int base_type = vf(fmt[index]);
        if (base_type == -1) {
            index++;
            continue; // Formato inválido ou desconhecido
        }
        
        type |= base_type;

        switch (type) {
            case 0x01:
                if (character(fp, va_arg(ap, char *)) == 0) count++;
                break;
            case 0x02:
                if (str(fp, va_arg(ap, char *)) == 0) count++;
                break;
            case 0x03:
                if (value(fp, va_arg(ap, int *), 4, 10) == 0) count++;
                break;
            case 0x04:
                if (value(fp, va_arg(ap, unsigned int *), 4, 10) == 0) count++;
                break;
            case 0x05:
                if (value(fp, va_arg(ap, unsigned int *), 4, 16) == 0) count++;
                break;
            case 0x06:
            case 0x07:
                if (fvalue(fp, va_arg(ap, float *), 4) == 0) count++;
                break;

            // Variantes long (64-bits)
            case 0x13:
                if (value(fp, va_arg(ap, long *), 8, 10) == 0) count++;
                break;
            case 0x14:
                if (value(fp, va_arg(ap, unsigned long *), 8, 10) == 0) count++;
                break;
            case 0x15:
                if (value(fp, va_arg(ap, unsigned long *), 8, 16) == 0) count++;
                break;
            case 0x16:
            case 0x17:
                if (fvalue(fp, va_arg(ap, double *), 8) == 0) count++;
                break;
            default:
                break;
        }

        index++;
    }

    return count;
}