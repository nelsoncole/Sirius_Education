/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: wakeup.c
 *    Description: Varre a árvore/lista global de processos do sistema à procura
 *                 de qualquer thread que esteja num estado de suspensão passiva
 *                 (THREAD_BLOCKED) e ancorada no mesmo canal de memória (chan).
 *                 Todas as correspondências encontradas são removidas do canal,
 *                 transitadas de volta para o estado ativo (THREAD_READY) e
 *                 reinseridas fisicamente na Runqueue do núcleo de processamento.
 * 
 *     Parameters: chan - Ponteiro opaco (âncora) do canal que emitiu o sinal de
 *                        desbloqueio (e.g., &tty->read_wait).
 * 
 *         Return:      None.
 * 
 *         Author: Nelson Cole
 *   Created Date: 05/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 06/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/kernel/sched/wakeup.h>
#include <kernel/klib.h>

void wakeup(void *chan) {
    if (chan == NULL) return;

    cpu_data_block_t *cpu = get_current_cpu();
    process_list_spinlock_acquire();

    // Varre todos os processos registados no sistema
    for (process_t* p = g_process_list_head; p != NULL; p = p->next) 
    {
        /*
         * NOTA DE ARQUITETURA: Se o processo tiver múltiplas threads, 
         * deve varrer a lista de threads do processo. Se por agora apenas 
         * suporta uma thread por processo, valida-se a main_thread.
         */
        if (p->main_thread != NULL && 
            p->main_thread->state == THREAD_WAITING && 
            p->main_thread->chan == chan) 
        {
            /*kprintf("[SCHED] wakeup: Acordando a Thread TID %d bloqueada no canal %p...\n", 
                    p->main_thread->tid, chan);*/
            
            /* 1. Remove a âncora de bloqueio */
            p->main_thread->chan = NULL;

            /* 2. Devolve o estado para READY */
            p->main_thread->state = THREAD_READY;

            /* 3. Insere a thread de volta na Runqueue do Core para voltar a executar */
            enqueue_thread(cpu, p->main_thread);
        }
    }

    process_list_spinlock_release();
}