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

void wait(void *chan, spinlock_t *lk) {
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

unsigned long wait_flag(void *chan, spinlock_t *lk, unsigned long flags) {
    if(!chan) return 0;
    
    unsigned long flags_local = spinlock_lock_irqsave(&g_wait_lock );
    cpu_data_block_t *cpu = get_current_cpu();
    thread_t *current = cpu->current_thread;
    
    current->chan = chan;
    current->state = THREAD_WAITING;

    /*kprintf("[SCHED] wait: Dormindo a Thread TID %d bloqueada no canal %p...\n", 
                    current->tid, chan);*/

    spinlock_unlock_irqrestore(&g_wait_lock , flags_local);
    
    // Liberta o lock do TTY para que o Mestre possa escrever no buffer
    if(lk != NULL) spinlock_unlock_irqrestore(lk, flags);
    
    schedule();
    
    // Quando a thread for acordada pelo wakeup, ela retoma aqui
    // e precisa de recuperar o lock antes de voltar para o loop do TTY

    if(lk != NULL) return spinlock_lock_irqsave(lk);

    return 0;
}

