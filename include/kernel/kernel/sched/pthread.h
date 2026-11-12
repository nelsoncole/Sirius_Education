
/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pthread.h
 *    Description: Protótipos e interfaces para criação e controlo de threads
 *                 POSIX (pthread).
 *
 *         Author: Nelson Cole
 *   Created Date: 07/11/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 07/11/2026
 *
 *        License: MIT
 * ============================================================================
 */

#ifndef _PTHREAD_H_
#define _PTHREAD_H_

#include <kernel/kernel/sched/scheduler.h>

/**
 * pthread - Subrotina interna de controlo para criação de threads.
 * @frame: Estrutura contendo o contexto completo dos registadores recuperados.
 *
 * Detalhes Técnicos:
 *   - Esta função encapsula as restrições lógicas da criação de threads
 *     do Sirius OS.
 *   - Invoca a sys_clone() para criar uma nova thread no processo atual,
 *     partilhando o espaço de memória e os recursos do processo.
 *   - A nova thread possui contexto de execução e stack próprios,
 *     mantendo a associação ao processo chamador.
 *
 * Retorno:
 *   - Retorna o TID da thread criada, ou -1 em caso de erro.
 */
pid_t pthread(stack_frame_t* frame);

#endif /* _PTHREAD_H_ */
