/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: idle.c
 *    Description: Implementação da tarefa de espera (Idle Task) do Kernel.
 *                 Garante economia de energia e resposta a interrupções.
 * 
 *         Author: Nelson Cole
 *   Created Date: 04/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 04/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/cpu/cpu.h>

/**
 * @brief Thread Idle do Kernel (Por núcleo).
 *        Executada autonomamente por cada CPU quando não existem processos 
 *        ou tarefas prontas na fila de agendamento (Runqueue).
 */
void cpu_idle(void) 
{
    for (;;) 
    {
        /* 
         * Ativa as interrupções locais para permitir que o núcleo 
         * acorde do estado de suspensão quando um evento disparar.
         */
        __asm__ volatile("sti"); 

        /* 
         * Coloca o processador em estado de baixo consumo (Halt).
         * O núcleo dorme até à próxima interrupção (ex: tique do LAPIC).
         */
        __asm__ volatile("hlt"); 
    }
}
