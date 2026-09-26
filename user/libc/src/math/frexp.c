/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: frexp.c
 *    Description: Implementação segura de frexp para extração de expoente e mantissa
 *                 em conformidade com as regras de strict-aliasing.
 * ============================================================================
 */

#include <math.h>
#include <stdint.h>

double frexp(double value, int *eptr)
{
    // Union para conversão segura de bits sem quebrar strict-aliasing
    union
    {
        double d;
        uint64_t i;
    } u;

    u.d = value;

    // Isola o expoente (bits 52 a 62 no padrão IEEE 754)
    int hx = (int)((u.i >> 52) & 0x7FF);

    if (hx == 0 || hx == 0x7FF)
    {
        // Se for 0, Infinito ou NaN, o expoente é 0 e retorna o próprio valor
        *eptr = 0;
        return value;
    }

    // Remove o bias do expoente (1023)
    *eptr = hx - 1022;

    // Força o expoente do double para 1022 (para que a mantissa fique no intervalo [0.5, 1.0[)
    u.i = (u.i & 0x800FFFFFFFFFFFFFULL) | (0x3FEULL << 52);

    return u.d;
}