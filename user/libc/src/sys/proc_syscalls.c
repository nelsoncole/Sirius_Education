/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: proc_syscalls.c
 *    Description: Implementação das chamadas de sistema (Syscalls) de gestão
 *                 de processos e tarefas em Ring 3.
 *                 Utiliza as macros universais inline de usyscall.h.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <unistd.h>
#include <sys/usyscall.h>

/**
 * _exit - Termina o processo atual de forma imediata e atómica.
 *         Comunica o status de retorno ao processo pai.
 * @status: O código de saída do processo (enviado para o waitpid do pai).
 */
void _exit(int status)
{
    /* 
     * Dispara a syscall 1 (SYS_EXIT) de forma inline.
     * Passa o registrador RAX = SYS_EXIT e RDI = status conforme a AMD64 ABI.
     */
    syscall1(SYS_EXIT, (uint64_t)status);

    /* 
     * SALVAGUARDA EXTREMA (Dead Code):
     * Caso a syscall falhe por um erro catastrófico no Kernel, a thread 
     * entra num loop infinito seguro com a instrução pause (Ring 3 Safe).
     * Isto impede que o ponteiro de instrução execute lixo na memória RAM.
     */
    while (1) 
    {
        __asm__ __volatile__("pause");
    }
}