/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: fork.h
 *    Description: Protótipos e interfaces para a rotina de bifurcação
 *                 clássica de processos (fork).
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

#ifndef _FORK_H_
#define _FORK_H_

#include <kernel/kernel/sched/scheduler.h>
#include <kernel/klib.h>

/**
 * fork - Subrotina interna de controlo para a duplicação do processo atual.
 * @frame: Estrutura contendo o contexto completo dos registadores recuperados.
 * 
 * Detalhes Técnicos:
 *   - Esta função encapsula as restrições lógicas do fork clássico do Sirius OS.
 *   - Força a ativação da flag CLONE_FILES ao invocar a sys_clone() para garantir
 *     que os descritores padrões (0, 1, 2) sejam corretamente vinculados e clonados.
 * 
 * Retorno:
 *   - Retorna o PID do processo filho gerado, ou -1 em caso de erro.
 */
pid_t fork(stack_frame_t* frame);

#endif /* _FORK_H_ */
