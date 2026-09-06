/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: scheduler.h
 *    Description: Cabeçalho do subsistema de agendamento (Task Scheduler).
 *                 Define as estruturas do Bloco de Controlo de Threads (TCB),
 *                 estados de execução e protótipos para o Multitasking em SMP.
 * 
 *         Author: Nelson Cole
 *   Created Date: 05/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 05/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include <kernel/lib/stdint.h>
#include "process.h"
#include "thread.h"
#include <kernel/arch/x86_64/cpu/cpu.h>

/* Declaração antecipada da estrutura do CPU para quebrar a dependência circular */
struct cpu_data_block;
typedef struct cpu_data_block cpu_data_block_t;

/*
 * ----------------------------------------------------------------------------
 * Protótipos das Funções do Agendador
 * ----------------------------------------------------------------------------
 */
void scheduler_init(void);
void* task_switch(void* regs); 
void enqueue_thread(cpu_data_block_t* cpu, thread_t* thread);
thread_t* dequeue_thread(cpu_data_block_t* cpu);

#endif /* _SCHEDULER_H_ */
