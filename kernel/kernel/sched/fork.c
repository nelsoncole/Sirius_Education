/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: fork.c
 *    Description: Subrotina interna de controlo para bifurcação de processos.
 *                 Interface especializada que expõe o comportamento do fork()
 *                 clássico. Invoca a lógica unificada de clonagem injetando
 *                 as restrições de isolamento e herança de arquivos de Ring 3.
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 26/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/sched/clone.h>
#include <kernel/kernel/sched/fork.h>
/**
 * sys_fork - Invoca o clone ativando explicitamente a flag CLONE_FILES.
 * @frame: Ponteiro para o Stack Frame contendo os registadores salvos da CPU.
 */
pid_t fork(stack_frame_t* frame)
{
    uint64_t saved_rsi = frame->rsi;

    // O fork ativa obrigatoriamente a flag CLONE_FILES
    frame->rsi = CLONE_FILES;

    pid_t result_pid = clone(frame);

    // Restaura o registador do Pai antes de retornar
    frame->rsi = saved_rsi;
    
    return result_pid;
}
