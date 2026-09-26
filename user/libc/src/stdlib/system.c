/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: system.c
 *    Description: Implementação regulamentar da função system para a LibC.
 *                 Usa as chamadas atómicas SYS_FORK, SYS_EXECVE e SYS_WAITPID.
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * ============================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/usyscall.h>

/**
 * system - Executa um comando do sistema passando-o para o interpretador nativo.
 * @string: O comando literal a ser executado (ex: "mkdir teste").
 * 
 * Retorna: O código de saída do comando executado, ou -1 em caso de falha grave.
 */
int system(const char *string)
{
    // REGRA DE OURO POSIX: Se string for NULL, retorna um valor diferente de zero
    // se o interpretador de comandos estiver disponível no sistema.
    if (string == NULL) {
        return 1; 
    }

    // 1. Dispara a duplicação atómica do processo atual via SYS_FORK
    pid_t pid = (pid_t)syscall0(SYS_FORK);

    if (pid < 0) {
        // Falha crítica ao criar a nova thread/processo no Kernel
        printf("SiriusOS: system: erro ao duplicar o processo (fork falhou).\n");
        return -1;
    }

    // ============================================================================
    // CONTEXTO DO PROCESSO FILHO
    // ============================================================================
    if (pid == 0) {
        // Vetor de argumentos obrigatórios para o execve:
        // Chamamos a Shell oficial "/bin/sh", passamos a flag "-c" e a string de comando.
        char *argv[] = { "/bin/sh", "-c", (char *)string, NULL };
        
        // Vetor de ambiente herdado (podes passar o teu vetor global 'environ')
        extern char **environ;

        // Substitui a imagem do processo atual pelo binário da Shell
        syscall3(SYS_EXECVE, (uint64_t)"/bin/sh", (uint64_t)argv, (uint64_t)environ);

        // Se o execve retornar, significa que o binário "/bin/sh" não foi encontrado no teu VFS!
        printf("SiriusOS: system: interpretador /bin/sh nao encontrado.\n");
        syscall1(SYS_EXIT, (uint64_t)127); // Código padrão POSIX para comando/shell não encontrada
        while(1); // Garante que o filho nunca regressa ao fluxo do pai
    }

    // ============================================================================
    // CONTEXTO DO PROCESSO PAI
    // ============================================================================
    int status = 0;
    
    // O pai bloqueia de forma síncrona aguardando que o filho termine a sua tarefa.
    // Invoca a chamada SYS_WAITPID passando: PID do filho, ponteiro de status e flags (0)
    pid_t wait_ret = (pid_t)syscall3(SYS_WAITPID, (uint64_t)pid, (uint64_t)&status, 0);

    if (wait_ret < 0) {
        return -1;
    }

    // Devolve o status de encerramento do processo filho
    return status;
}