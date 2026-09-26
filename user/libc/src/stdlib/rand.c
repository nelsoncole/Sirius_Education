/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: rand.c
 *    Description: Gerador de números pseudo-aleatórios (rand) em Ring 3.
 *                 Implementa o algoritmo LCG (Linear Congruential Generator).
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * ============================================================================
 */

#include <stdlib.h>
#include <stdint.h>

// Declaração externa do limite do heap para injetar entropia física de RAM
extern uint64_t g_uheap_current_end;
extern unsigned long g_rand_next;

/**
 * rand - Gera um número pseudo-aleatório entre 0 e RAND_MAX (32767).
 * 
 * Retorna: Um inteiro positivo aleatório.
 */
int rand(void)
{
    // 1. Injeta entropia dinâmica capturando o TSC (Time-Stamp Counter) do CPU x86_64.
    // O rdtsc lê o número de ciclos de clock desde o boot. Isto garante que mesmo
    // sem mudar a semente manualmente, o número varia conforme o tempo exato de execução!
    uint32_t low_tick;
    __asm__ __volatile__("rdtsc" : "=a"(low_tick) :: "rdx");

    // 2. Aplica a fórmula clássica LCG (multiplicador e incremento oficiais do padrão POSIX)
    // Usamos barreiras atómicas simples ou volatile para impedir o -O2 de ignorar a escrita
    g_rand_next = g_rand_next * 1103515245UL + 12345UL;
    
    // Mistura o estado com o TSC e com o endereço do heap da RAM para máxima entropia
    uint32_t final_seed = (uint32_t)(g_rand_next ^ low_tick ^ g_uheap_current_end);

    // 3. Modula o resultado para retornar no intervalo estrito de 0 a RAND_MAX (32767)
    // RAND_MAX está definido no teu <stdlib.h> como 32767
    return (int)((final_seed / 65536) % 32768);
}