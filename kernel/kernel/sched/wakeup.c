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
 *   Created Date: 24/10/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 26/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/kernel/sched/wakeup.h>
#include <kernel/kernel/sched/wait.h>
#include <kernel/klib.h>

extern spinlock_t g_wait_lock;

void wakeup(void *chan) {
    if (chan == NULL) return;
    
    unsigned long flags = spinlock_lock_irqsave(&g_wait_lock);

    wait_queue_t *node = wait_queue_remove(chan);

    if (node)
    {
        /*kprintf("[SCHED] wakeup: Acordando a Thread TID %d bloqueada no canal %p...\n",
                    node->thread->tid, chan);*/

        /* 1. Remove a âncora de bloqueio */
        node->thread->chan = NULL;

        /* 2. Devolve o estado para READY */
        node->thread->state = THREAD_READY;

        /* 3. Insere a thread de volta na Runqueue do Core para voltar a executar */
        enqueue_thread(node->cpu, node->thread);
        kfree(node);
    }
    spinlock_unlock_irqrestore(&g_wait_lock , flags);
}

void wakeup_thread(void *chan, void *th) {
    if (!chan || !th ) return;

    unsigned long flags_local = spinlock_lock_irqsave(&g_wait_lock );

    thread_t *thread = (thread_t*)th;
     if (thread != NULL && 
            thread->state == THREAD_WAITING && 
            thread->chan == chan) 
        {

            /*kprintf("[SCHED] wakeup: Acordando a Thread TID %d bloqueada no canal %p...\n", 
                    thread->tid, chan);*/
            
            thread->chan = NULL;

            thread->state = THREAD_READY;
            
            cpu_data_block_t *cpu = get_cpu_data_block(thread->cpu_id);
            enqueue_thread(cpu, thread);
        }
    spinlock_unlock_irqrestore(&g_wait_lock , flags_local);
}