/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: system.c
 *    Description: Implementação regulamentar da função system para a LibC.
 *                 Utiliza as chamadas portáveis de alto nível fork, execve,
 *                 _exit e waitpid da unistd.h e sys/wait.h.
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * ============================================================================
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

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

    // 1. Dispara a duplicação atómica do processo atual utilizando a API de alto nível
    pid_t pid = fork();

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
        
        // Vetor de ambiente herdado através do ponteiro de ambiente global
        extern char **environ;

        // Substitui a imagem do processo atual pelo binário da Shell de forma limpa
        execve("/bin/sh", argv, environ);

        // Se o execve retornar, significa que o binário "/bin/sh" não foi encontrado no teu VFS!
        printf("SiriusOS: system: interpretador /bin/sh nao encontrado.\n");
        
        // Encerra de forma imediata e atómica o processo filho falhado
        _exit(127); // Código padrão POSIX para comando/shell não encontrada
    }

    // ============================================================================
    // CONTEXTO DO PROCESSO PAI
    // ============================================================================
    int status = 0;
    
    // O pai bloqueia de forma síncrona aguardando que o filho termine a sua tarefa.
    // Substitui o syscall3 primitivo pela chamada portátil waitpid() da sys/wait.h
    pid_t wait_ret = waitpid(pid, &status, 0);

    if (wait_ret < 0) {
        return -1;
    }

    // Devolve o status de encerramento do processo filho recuperado pelo Kernel
    return status;
}