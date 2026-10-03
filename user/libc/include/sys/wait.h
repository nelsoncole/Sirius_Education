/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: wait.h
 *    Description: Declarações e macros regulamentares para sincronização e
 *                 espera de processos filhos (waitpid) em Ring 3.
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _WAIT_H_
#define _WAIT_H_

#include <sys/types.h> // Garante o reconhecimento do tipo pid_t

/* Options para a função waitpid */
#define WNOHANG    0x00000001  /* Retorna imediatamente se nenhum filho tiver terminado */
#define WUNTRACED  0x00000002  /* Também relata filhos que foram parados por um sinal */

/* 
 * MACROS DE AVALIAÇÃO DE STATUS (POSIX)
 * Permitem ao processo pai inspecionar o inteiro 'wstatus' retornado pelo Kernel.
 */
#define WIFEXITED(status)    (((status) & 0x7F) == 0)           /* Terminou normalmente via exit? */
#define WEXITSTATUS(status)  (((status) >> 8) & 0xFF)           /* Captura o código de saída (0-255) */
#define WIFSIGNALED(status)  (((status) & 0x7F) > 0 && ((status) & 0x7F) < 0x7F) /* Terminou por sinal? */
#define WTERMSIG(status)     ((status) & 0x7F)                  /* Captura o número do sinal fatal */

/**
 * waitpid - Bloqueia de forma síncrona o processo atual até que o filho especificado
 *           mude de estado (encerre, seja parado ou continue).
 * 
 * @pid:      O PID do processo alvo. 
 *            Se -1, aguarda por qualquer processo filho (estilo wait comum).
 * @wstatus:  Ponteiro para um inteiro onde o Kernel injetará o status de encerramento.
 * @options:  Flags binárias de controlo (ex: 0 ou WNOHANG).
 * 
 * Retorna: O PID do filho que mudou de estado, 0 se WNOHANG foi usado e nenhuma 
 *          tarefa mudou, ou -1 em caso de erro grave (ex: PID não existe).
 */
pid_t waitpid(pid_t pid, int *wstatus, int options);

pid_t wait(int *wstatus);

#endif /* _WAIT_H_ */
