/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: timer.h
 *    Description: Interface de Abstração de Hardware para gestão de tempo
 *                 e delays de alta precisão baseados no Invariant TSC.
 * 
 *         Author: Nelson Cole
 *   Created Date: 18/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 18/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _TIMER_H_
#define _TIMER_H_

#include <kernel/lib/stdint.h>

// Frequência do TSC calculada dinamicamente em Hz durante a inicialização
extern uint64_t g_tsc_hz;

// Lê o TSC aplicando uma barreira de serialização (lfence) contra execução fora de ordem
static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("lfence\n\t"
                     "rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

// Inicializa o subsistema de tempo e calibra o TSC através do ACPI PM Timer
void timer_init(void);

// Funções de atraso de alta precisão por espera ativa (busy-wait)
void ndelay(uint64_t nsecs);
void udelay(uint64_t usecs);
void mdelay(uint64_t msecs);

#endif