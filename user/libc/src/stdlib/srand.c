/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: rand.c
 *    Description: Gerador de números pseudo-aleatórios (srand) em Ring 3.
 *                 Implementa o algoritmo LCG (Linear Congruential Generator).
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * ============================================================================
 */

#include <stdlib.h>
#include <stdint.h>

// Semente global interna (padrão ISO C inicia em 1)
unsigned long g_rand_next = 1;

/**
 * srand - Define a semente inicial para o gerador pseudo-aleatório.
 * @seed: Valor da nova semente.
 */
void srand(unsigned int seed)
{
    g_rand_next = seed;
}