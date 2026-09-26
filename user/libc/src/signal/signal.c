/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: signal.c
 *    Description: Stubs temporários de manipulação de sinais POSIX.
 *                 Garante a captura atómica de falhas de Ring 3 entrando em
 *                 pânico controlado antes da integração com o micronúcleo.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 25/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <signal.h>
#include <stdio.h>

/**
 * signal - Estabelece uma rotina de tratamento para um sinal específico.
 * @signum:  O identificador numérico do sinal (ex: SIGSEGV, SIGINT).
 * @handler: Ponteiro para a função de tratamento ou macros (SIG_IGN/SIG_DFL).
 * @return:  O manipulador anterior, ou SIG_ERR em caso de erro grave.
 */
sighandler_t signal(int signum, sighandler_t handler)
{
    // CORREÇÃO: Especificadores adequados (%d para int, %p para ponteiro de função)
    printf("panic: libc_signal( signum: %d, handler: %p ) disparado!\n", signum, (void *)handler);
    
    // Bloqueia a execução da thread em Ring 3 para depuração do estado dos registos
    for (;;) {
        __asm__ __volatile__("pause");
    }

    return SIG_ERR; 
}

/**
 * sigaction - Altera e inspeciona de forma detalhada a ação associada a um sinal.
 * @signum: O identificador numérico do sinal.
 * @act:    Estrutura contendo a nova política e máscaras de bloqueio.
 * @oldact: Estrutura onde o Kernel guardará a política anterior.
 * @return: 0 em caso de sucesso, ou -1 em caso de erro.
 */
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    (void)act;
    (void)oldact;

    printf("panic: libc_sigaction( signum: %d, act: %p, oldact: %p ) disparado!\n", 
           signum, (const void *)act, (void *)oldact);

    for (;;) {
        __asm__ __volatile__("pause");
    }

    return -1;
}

