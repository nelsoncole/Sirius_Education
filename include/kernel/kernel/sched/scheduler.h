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

/**
 * Adiciona uma thread ao fim da fila de prontos de um CPU específico.
 */
void enqueue_thread(cpu_data_block_t* cpu, thread_t* thread);
/**
 * Remove e retorna a próxima thread do início da fila de prontos de um CPU.
 */
thread_t* dequeue_thread(cpu_data_block_t* cpu);
/**
 * Insere uma thread terminada (THREAD_DEAD) na fila de descarte local do CPU.
 * Nota: Garanta que adicionou 'dead_queue_head' e 'dead_queue_tail' na sua 'cpu_data_block_t'.
 */
void enqueue_dead_thread(cpu_data_block_t* cpu, thread_t* thread);
/**
 * Varre a lista de threads mortas do CPU atual e desaloca a memória física de cada uma.
 * Esta função é chamada de forma segura em contextos onde nenhuma thread em execução 
 * está a depender das pilhas a serem eliminadas.
 */
void scheduler_reclaim_dead_threads(void);
/**
 * Rotina da Thread Ociosa (Idle Thread).
 * Executada continuamente quando não existem outras tarefas prontas.
 * Agora, atua também como o Coletor de Lixo básico do núcleo.
 */
void idle_thread_routine(void);
/**
 * Inicializa o agendador local para o núcleo de processamento atual.
 * Deve ser chamada individualmente pelo BSP e por cada AP no arranque.
 */
void scheduler_init(void);
/**
 * scheduler_ready_process - Ativa o processo e insere a sua thread principal
 *                          na fila de execução da CPU correta de forma segura.
 * @proc: O ponteiro para o PCB do processo que acabou de ser configurado.
 */
void scheduler_ready_process(struct process* proc);
/**
 * Realiza a troca de contexto local do núcleo (Task Switch).
 * Chamada de dentro do handler do Timer do LAPIC (Interrupção 32).
 */
void* task_switch(void* regs);
/**
 * Encerra voluntariamente o processo atual, liberta o seu espaço de endereçamento 
 * e remove-o permanentemente da fila de execução do Escalonador (Scheduler).
 * 
 * NOTA DE ARQUITETURA: Esta função assume o controlo da Stack e NUNCA mais retorna.
 * 
 * @param code Código de status de finalização que será reportado ao processo pai.
 */
void scheduler_exit(int code);

/**
 * scheduler_yield - Permite que uma Thread de Kernel (KThread) abdique voluntariamente
 *                   do processador, devolvendo o controlo ao escalonador de imediato.
 */
void scheduler_yield(void);


#endif /* _SCHEDULER_H_ */
