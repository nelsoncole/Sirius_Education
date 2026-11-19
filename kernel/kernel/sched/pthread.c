/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pthread.c
 *    Description: Subrotina interna de controlo para criação de threads POSIX.
 *                 Interface especializada que expõe o comportamento de
 *                 pthread_create(). Invoca a lógica unificada de clonagem,
 *                 permitindo a criação de uma nova thread que partilha os
 *                 recursos e o espaço de memória do processo chamador.
 *
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 07/11/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/sched/clone.h>
#include <kernel/kernel/sched/pthread.h>
#include <kernel/arch/x86_64/kapi/irq.h>
#include <kernel/kernel/sched/wait.h>
#include <kernel/kernel/sched/wakeup.h>
#include <kernel/kernel/core/spinlock.h>

/**
 * sys_pthread - Invoca o clone para criar uma nova thread no processo atual.
 * @frame: Ponteiro para o Stack Frame contendo os registadores salvos da CPU.
 */
pid_t pthread(stack_frame_t* frame)
{
    uint64_t saved_rsi = frame->rsi;

    frame->rsi = CLONE_VM;

    pid_t result_tid = clone(frame);

    // Restaura o registador do Pai antes de retornar
    frame->rsi = saved_rsi;

    return result_tid;
}


void pthread_join(thread_t *target, void **retval)
{
    if (!target)
        return;

    cpu_data_block_t *cpu = get_current_cpu();
    thread_t *current = cpu->current_thread;

    if (!current || current == target)
        return;

    process_t *process = current->owner;

    spin_lock(&process->thread_lock);

    if (!process_has_thread(process, target, NULL)) {
        spin_unlock(&process->thread_lock);
        return;
    }

    /*
     * Enquanto a thread não terminar:
     * bloquear a thread atual e aguardar a sua conclusão.
     */
    while (!target->exited)
        wait(target, &process->thread_lock);

    /*
     * Recuperar o retval.
     */

    if (retval)
        *retval = target->retval;

    /*
     * Marcar como recolhida.
     */
    target->joined = true;

    spin_unlock(&process->thread_lock);
}

void pthread_exit(thread_t *thread, void *retval)
{
    if (!thread)
        return;

    spin_lock(&thread->owner->thread_lock);

    cpu_data_block_t *cpu = get_current_cpu();

    thread->retval = retval;
    thread->exited = true;
    thread->state = THREAD_DEAD;

    enqueue_dead_thread(cpu, thread);

    /*
     * Acordar a thread que está à espera através de pthread_join().
     */

    wakeup(thread);
    
    spin_unlock(&thread->owner->thread_lock);

    schedule();

    for (;;)
        __asm__ __volatile__("sti; hlt" ::: "memory");
}
