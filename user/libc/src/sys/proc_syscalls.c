/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: proc_syscalls.c
 *    Description: Implementação das chamadas de sistema (Syscalls) de gestão
 *                 de processos, identidade, fluxo de execução e sincronismo.
 *                 Utiliza as macros universais inline de usyscall.h.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <unistd.h>
#include <stdarg.h>
#include <sys/usyscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * 1. GESTÃO DE PROCESSOS E FLUXO DE EXECUÇÃO
 * ============================================================================
 */

/**
 * _exit - Termina o processo atual de forma imediata e atómica.
 */
void _exit(int status)
{
    syscall1(SYS_EXIT, (uint64_t)status);

    /* Salvaguarda contra corrupção ou falha catastrófica */
    while (1) 
    {
        __asm__ __volatile__("pause");
    }
}

/**
 * fork - Cria um processo filho duplicando o contexto do processo pai.
 */
pid_t fork(void)
{
    return (pid_t)syscall0(SYS_FORK);
}

/**
 * execve - Substitui o binário do processo atual por um novo executável.
 *          Ponto de entrada nativo da Syscall no Kernel.
 */
int execve(const char *pathname, char *const argv[], char *const envp[])
{
    char new_pathname[PATH_MAX];
    if (pathname[0] == '/')
    {
        strcpy(new_pathname, pathname);
    }
    else
    {
        char pwd[PATH_MAX];
        if (getcwd(pwd, PATH_MAX) == NULL)
        {
            return -1;
        }
        else
        {
            sprintf(new_pathname, "%s/%s", pwd, pathname);
        }
    }

    return (int)syscall3(SYS_EXECVE, (uint64_t)new_pathname, (uint64_t)argv, (uint64_t)envp);
}

/**
 * execv - Wrapper conveniente para execve omitindo variáveis de ambiente.
 */
int execv(const char *pathname, char *const argv[])
{
    /* Mapeia o vetor de ambiente global padrão (extern char **environ) como nulo */
    return execve(pathname, argv, NULL);
}

/**
 * execl - Wrapper para executar um binário passando argumentos via Lista Variádica.
 */
int execl(const char *pathname, const char *arg, ...)
{
    va_list args;
    va_start(args, arg);

    /* 1. Conta o número de argumentos para alocar o vetor temporário */
    int count = 0;
    if (arg != NULL) 
    {
        count++;
        const char* tmp;
        while ((tmp = va_arg(args, const char*)) != NULL) 
        {
            count++;
        }
    }
    va_end(args);

    /* 2. Cria o vetor argv baseado no tamanho descoberto (+1 para o terminador NULL) */
    char* argv[count + 1];
    
    va_start(args, arg);
    if (arg != NULL)
    {
        argv[0] = (char*)arg;
        for (int i = 1; i < count; i++)
        {
            argv[i] = va_arg(args, char*);
        }
    }
    argv[count] = NULL; /* Terminador estrito obrigatório */
    va_end(args);

    /* 3. Desvia a execução para o ponto nativo */
    return execve(pathname, argv, NULL);
}

// Dentro de proc_syscalls.c (Ring 3):
pid_t waitpid(pid_t pid, int *wstatus, int options)
{
    return (pid_t)syscall3(SYS_WAITPID, (uint64_t)pid, (uint64_t)wstatus, (uint64_t)options);
}

pid_t wait(int *wstatus)
{
    // Passar -1 diz à tua sys_waitpid para libertar o primeiro zombie que encontrar
    return waitpid(-1, wstatus, 0);
}



/* ============================================================================
 * 2. IDENTIFICAÇÃO E UTILIDADES DE PROCESSOS
 * ============================================================================
 */

/**
 * getpid - Obtém o ID do processo atual.
 */
pid_t getpid(void)
{
    return (pid_t)syscall0(SYS_GETPID);
}

/**
 * getppid - Obtém o ID do processo pai.
 */
pid_t getppid(void)
{
    return (pid_t)syscall0(SYS_GETPPID);
}

/**
 * getuid - Obtém o ID do utilizador real do processo.
 */
uid_t getuid(void)
{
    return (uid_t)syscall0(SYS_GETUID);
}

/**
 * getgid - Obtém o ID do grupo real do processo.
 */
gid_t getgid(void)
{
    return (gid_t)syscall0(SYS_GETGID);
}

/**
 * setuid - Define o ID do utilizador ativo.
 */
int setuid(uid_t uid)
{
    return (int)syscall1(SYS_SETUID, (uint64_t)uid);
}

/**
 * setgid - Define o ID do grupo ativo.
 */
int setgid(gid_t gid)
{
    return (int)syscall1(SYS_SETGID, (uint64_t)gid);
}

/* ============================================================================
 * 3. UTILITÁRIOS DE TEMPO E SINCRONISMO
 * ============================================================================
 */

/**
 * sleep - Suspende a execução da thread atual por um período em segundos.
 */
unsigned int sleep(unsigned int seconds)
{
    /* Invoca o serviço de bloqueio temporal do Kernel */
    return (unsigned int)syscall1(SYS_SLEEP, (uint64_t)seconds);
}

/**
 * usleep - Suspende a execução da thread atual por um período em microssegundos.
 */
int usleep(unsigned int usec)
{
    /* Mapeia para a syscall especializada do kernel para micro-latências */
    return (int)syscall1(SYS_USLEEP, (uint64_t)usec);
}

/**
 * Altera o diretório de trabalho atual do processo.
 * Invoca internamente a syscall sys_chdir.
 */
int chdir(const char *path) 
{
    if (!path) return -1;
    
    /* Dispara a chamada de sistema passando o ponteiro do caminho */
    int ret = (int)syscall1(SYS_CHDIR, (uint64_t)path);
    
    // Se o kernel retornar um valor negativo (erro), podes mapear para o errno aqui
    if (ret < 0) {
        // errno = -ret; (Opcional, se usares a variável global errno)
        return -1;
    }
    
    return 0; // Sucesso
}

/**
 * Lê o diretório de trabalho atual do processo.
 * Invoca internamente a syscall sys_getcwd.
 */
char *getcwd(char *buf, size_t size) 
{
    /* 
     * COMPATIBILIDADE POSIX EXTENDIDA:
     * Se buf for NULL, a especificação dita que a libc deve alocar dinamicamente 
     * o buffer na Heap do utilizador com o tamanho solicitado (ou PATH_MAX).
     */
    char *target_buf = buf;
    int allocated = 0;

    if (!target_buf) {
        size_t alloc_size = (size == 0) ? 256 : size; // Usamos o teu limite de 256 bytes
        target_buf = (char *)malloc(alloc_size);
        if (!target_buf) return NULL;
        size = alloc_size;
        allocated = 1;
    }

    /* Invoca a chamada de sistema especializada sys_getcwd passando o buffer e o limite */
    int ret = (int)syscall2(SYS_GETCWD, (uint64_t)target_buf, (uint64_t)size);

    if (ret < 0) {
        // Se alocámos a memória nesta chamada e o kernel falhou, libertamos para evitar memory leak
        if (allocated) free(target_buf);
        // errno = -ret;
        return NULL;
    }

    return target_buf; // Retorna o ponteiro para a string com o caminho absoluto
}