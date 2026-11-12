
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
