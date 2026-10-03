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
 *    Modified By: Nelson Cole
 *  Modified Date: 01/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

/**
 * signal - Estabelece uma rotina de tratamento para um sinal específico.
 * @signum:  O identificador numérico do sinal (ex: SIGSEGV, SIGINT).
 * @handler: Ponteiro para a função de tratamento ou macros (SIG_IGN/SIG_DFL).
 * @return:  O manipulador anterior, ou SIG_ERR em caso de erro grave.
 */
sighandler_t signal(int signum, sighandler_t handler)
{
    // Especificadores adequados (%d para int, %p para ponteiro de função)
    printf("panic: libc_signal( signum: %d, handler: %p ) disparado!\n", signum, (void *)handler);
    
    _exit(signum);

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

    _exit(signum);

    return -1;
}

/**
 * raise - Envia um sinal atómico direcionado ao próprio processo atual.
 *         Adicionado para capturar falhas de sanidade interna (como o SIGABRT)
 *         geradas pela amálgama da LibTomCrypt no Ring 3.
 * @sig:   O identificador numérico do sinal a ser disparado.
 * @return: 0 em caso de sucesso, ou valor não-zero em erro.
 */
int raise(int sig)
{
    printf("libc: raise( sig: %d ) disparado. Encerrando processo Ring 3...\n", sig);

    _exit(sig); 

    return -1; // Nunca será alcançado
}
