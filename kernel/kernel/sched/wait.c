/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: wait.c
 *    Description: Bloqueia a execução da tarefa/thread atual, colocando-a num
 *                 estado de espera passiva (THREAD_BLOCKED) associada a um canal
 *                 de sincronização específico (âncora de memória).
 *                 Liberta o spinlock do dispositivo/recurso de forma segura para
 *                 evitar condições de corrida (Deadlocks) e invoca o agendador
 *                 para ceder o tempo de CPU a outra tarefa pronta.
 * 
 *     Parameters: chan - Ponteiro opaco (âncora) que identifica o canal de espera.
 *                 lk   - Ponteiro para o spinlock do recurso que será libertado
 *                        durante o repouso e readquirido ao acordar.
 * 
 *         Return:  None (Retorna apenas quando a thread for reativada por um wakeup).
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
#include <kernel/kernel/sched/wait.h>
#include <kernel/klib.h>

spinlock_t g_wait_lock = {0};
spinlock_t g_queue_lock = {0};

wait_queue_t *g_wait_head = NULL;
wait_queue_t *g_wait_tail = NULL;

static void wait_queue_add(cpu_data_block_t *cpu, thread_t *thread, void *chan)
{
    wait_queue_t *node = kmalloc(sizeof(wait_queue_t));

    if (!node)
        return;

    node->cpu = cpu;
    node->thread = thread;
    node->chan = chan;

    spinlock_acquire(&g_queue_lock);

    if (!g_wait_head) {
        g_wait_head = node;
        g_wait_tail = node;
        node->next = node;
    } else {
        node->next = g_wait_head;
        g_wait_tail->next = node;
        g_wait_tail = node;
    }

    spinlock_release(&g_queue_lock);
}

wait_queue_t *wait_queue_remove(void *chan)
{
    if (!g_wait_head || !chan)
        return NULL;

    wait_queue_t *current = g_wait_head;
    wait_queue_t *previous = g_wait_tail;

    spinlock_acquire(&g_queue_lock);
    do {
        if (current->chan == chan)
        {
            if (current == g_wait_head && current == g_wait_tail)
            {
                g_wait_head = NULL;
                g_wait_tail = NULL;
            }
            else
            {
                previous->next = current->next;

                if (current == g_wait_head)
                    g_wait_head = current->next;

                if (current == g_wait_tail)
                    g_wait_tail = previous;
            }

            current->next = NULL;
            spinlock_release(&g_queue_lock);
            return current;
        }

        previous = current;
        current = current->next;

    } while (current != g_wait_head);

    spinlock_release(&g_queue_lock);

    return NULL;
}

void wait(void *chan, spinlock_t *lk) {
    if(!chan) return;

    unsigned long flags = spinlock_lock_irqsave(&g_wait_lock);
    cpu_data_block_t *cpu = get_current_cpu();
    thread_t *current = cpu->current_thread;


    wait_queue_add(cpu, current, chan);
    
    current->chan = chan; // Guarda a âncora (ex: &tty->read_wait)
    current->state = THREAD_WAITING;

    spinlock_unlock_irqrestore(&g_wait_lock , flags);
    
    // Liberta o lock do TTY para que o Mestre possa escrever no buffer
    if(lk != NULL) spin_unlock(lk);

    /*kprintf("[SCHED] wait: Dormindo a Thread TID %d bloqueada no canal %p...\n", 
                    current->tid, chan);*/
    
    schedule();
    
    // Quando a thread for acordada pelo wakeup, ela retoma aqui
    // e precisa de recuperar o lock antes de voltar para o loop do TTY

    if(lk != NULL) spin_lock(lk);
}


void wait_no_queue(void *chan, spinlock_t *lk) {
    if(!chan) return;

    unsigned long flags = spinlock_lock_irqsave(&g_wait_lock);
    cpu_data_block_t *cpu = get_current_cpu();
    thread_t *current = cpu->current_thread;

    
    current->chan = chan; // Guarda a âncora (ex: &tty->read_wait)
    current->state = THREAD_WAITING;

    spinlock_unlock_irqrestore(&g_wait_lock , flags);
    
    // Liberta o lock do TTY para que o Mestre possa escrever no buffer
    if(lk != NULL) spin_unlock(lk);

    /*kprintf("[SCHED] wait: Dormindo a Thread TID %d bloqueada no canal %p...\n", 
                    current->tid, chan);*/
    
    schedule();
    
    // Quando a thread for acordada pelo wakeup, ela retoma aqui
    // e precisa de recuperar o lock antes de voltar para o loop do TTY

    if(lk != NULL) spin_lock(lk);
}

