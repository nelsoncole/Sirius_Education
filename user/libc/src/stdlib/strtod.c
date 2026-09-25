/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: strtod.c
 *    Description: Converte uma string de caracteres num valor double.
 *                 Inclui tratamento de expoentes, sinais e limites flutuantes.
 * 
 *        Credits: Michael Ringgaard / Nelson Cole
 *   Created Date: 24/09/2026
 * ============================================================================
 */

#include <stdlib.h>
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <errno.h> 

#undef strtod

/**
 * Interface limpa e nativa de 64-bits para gerar o Infinito IEEE 754
 * sem depender de ficheiros Assembly autónomos em Ring 3.
 */
#define HUGE_VAL (__builtin_inf())

double strtod(const char *str, char **endptr) {
    double number;
    int exponent;
    int negative;
    char *p = (char *) str;
    double p10;
    int n;
    int num_digits;
    int num_decimals;

    // Ignora espaços em branco iniciais
    while (isspace(*p)) p++;

    // Trata o sinal opcional
    negative = 0;
    switch (*p) {
        case '-': 
            negative = 1; 
            __attribute__((fallthrough)); // CORREÇÃO: Avisa formalmente o GCC que o salto é intencional
        case '+': 
            p++;
            break; // Boa prática defensiva de fim de switch
    }

    number = 0.;
    exponent = 0;
    num_digits = 0;
    num_decimals = 0;

    // Processa a sequência de dígitos inteiros
    while (isdigit(*p)) {
        number = number * 10. + (*p - '0');
        p++;
        num_digits++;
    }

    // Processa a parte decimal fracionária
    if (*p == '.') {
        p++;

        while (isdigit(*p)) {
            number = number * 10. + (*p - '0');
            p++;
            num_digits++;
            num_decimals++;
        }

        exponent -= num_decimals;
    }

    if (num_digits == 0) {
        return 0.0;
    }

    // Corrige o sinal do número gerado
    if (negative) number = -number;

    // Processa a sequência de string de expoente (e/E)
    if (*p == 'e' || *p == 'E') {
        negative = 0;
        switch (*++p) {
            case '-': 
                negative = 1;   
                __attribute__((fallthrough)); // CORREÇÃO: Silencia o aviso do GCC de forma regulamentar
            case '+': 
                p++;
                break;
        }

        // Processa os dígitos do expoente
        n = 0;
        while (isdigit(*p)) {
            n = n * 10 + (*p - '0');
            p++;
        }

        if (negative) {
            exponent -= n;
        } else {
            exponent += n;
        }
    }

    // Validação de estouro contra as propriedades físicas do hardware (float.h)
    if (exponent < DBL_MIN_EXP || exponent > DBL_MAX_EXP) {
        errno = ERANGE;
        return (double) HUGE_VAL;
    }

    // Multiplica ou divide para ajustar a escala decimal final (Best-Fit Scale)
    p10 = 10.;
    n = exponent;
    if (n < 0) n = -n;
    while (n) {
        if (n & 1) {
            if (exponent < 0) {
                number /= p10;
            } else {
                number *= p10;
            }
        }
        n >>= 1;
        p10 *= p10;
    }

    if (number == HUGE_VAL) {
        errno = ERANGE;
    }

    // Atualiza o ponteiro de paragem do leitor caso o utilizador tenha solicitado
    if (endptr) *endptr = p;

    return number;
}