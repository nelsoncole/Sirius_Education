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
 *  Modified Date: 23/11/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/scheduler.h>
#include <kernel/kernel/sched/wakeup.h>
#include <kernel/kernel/sched/wait.h>
#include <kernel/klib.h>

extern spinlock_t g_wait_lock;
extern spinlock_t g_wait_no_queue_lock;

void wakeup(void *chan)
{
    if (!chan)
        return;

    unsigned long flags =
        spinlock_lock_irqsave(&g_wait_lock);

    wait_queue_t *node;

    while ((node = wait_queue_find(chan)) != NULL)
    {
        thread_t *thread = node->thread;

        if (thread &&
            thread->state == THREAD_WAITING &&
            thread->chan == chan)
        {
            if (wait_queue_remove_node(node))
            {
                //kprintf("wakeup tid %ld chan %p cpu id %d\n", thread->tid, thread->chan, thread->cpu_id);

                thread->chan = NULL;
                thread->state = THREAD_READY;
                enqueue_thread(node->cpu, thread);
                // Nota:
                // O node e liberado dentro do wait
                //kfree(node);

            }else 
            {
                kprintf("no wakeup tid %ld chan %p cpu id %d\n", thread->tid, thread->chan, thread->cpu_id);
            }
        }
        else
        {
            break;
        }
    }

    spinlock_unlock_irqrestore(&g_wait_lock, flags);
}

void wakeup_no_queue(void *chan, void *th)
{
    if (!chan || !th)
        return;

    unsigned long flags =
        spinlock_lock_irqsave(&g_wait_no_queue_lock);

    thread_t *thread = (thread_t *)th;

    if (thread->state == THREAD_WAITING &&
        thread->chan == chan)
    {
        thread->chan = NULL;
        thread->state = THREAD_READY;

        cpu_data_block_t *cpu =
            get_cpu_data_block(thread->cpu_id);

        if (cpu)
            enqueue_thread(cpu, thread);
    }

    spinlock_unlock_irqrestore(&g_wait_no_queue_lock, flags);
}