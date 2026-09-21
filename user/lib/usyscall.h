/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: usyscall.h
 *    Description: Invólucros universais em Assembly Inline para a instrução
 *                 nativa 'syscall' em ambiente x86_64 (Ring 3).
 *                 Respeita a System V AMD64 ABI e protege RCX/R11.
 *
 *         Author: Nelson Cole
 *   Created Date: 17/09/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 17/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#ifndef _USYSCALL_H_
#define _USYSCALL_H_

/* Operações Básicas de I/O, Memória e Execução */
#define SYS_READ   0
#define SYS_WRITE  1
#define SYS_BRK    2
#define SYS_EXIT   3

/* Expansão das operações do Sistema de Ficheiros Virtual (VFS) */
#define SYS_MOUNT   4
#define SYS_UMOUNT  5
#define SYS_OPEN    6
#define SYS_CLOSE   7
#define SYS_SEEK    8
#define SYS_FLUSH   9
#define SYS_STAT    10
#define SYS_CHMOD   11
#define SYS_UNLINK  12
#define SYS_RMDIR   13
#define SYS_RENAME  14
#define SYS_IOCTL   15

/* Gestão de Processos e Memória Avançada */
#define SYS_FORK    16
#define SYS_EXECVE  17
#define SYS_MMAP    18
#define SYS_MUNMAP  19
#define SYS_GETPID  20
#define SYS_GETPPID 21

/* Sincronização, Tempo e Sinais */
#define SYS_WAITPID   22
#define SYS_SLEEP     23
#define SYS_KILL      24
#define SYS_SIGACTION 25

/* Subsistema de Sockets e Rede */
#define SYS_SOCKET      26
#define SYS_BIND        27
#define SYS_LISTEN      28
#define SYS_ACCEPT      29
#define SYS_CONNECT     30
#define SYS_SEND        31
#define SYS_RECV        32
#define SYS_SENDTO      33
#define SYS_RECVFROM    34
#define SYS_SHUTDOWN    35
#define SYS_SETSOCKOPT  36
#define SYS_GETSOCKOPT  37

typedef unsigned long uint64_t;

/**
 * @brief Syscall com 0 argumentos.
 * RAX = Número da Syscall
 */
static inline uint64_t syscall0(uint64_t num) 
{
    uint64_t ret;
    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 1 argumento (A que a sua sbrk_user precisa!).
 * RAX = Número da Syscall, RDI = Argumento 1
 */
static inline uint64_t syscall1(uint64_t num, uint64_t arg1) 
{
    uint64_t ret;
    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1) // "D" força o GCC a colocar arg1 diretamente em RDI
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 2 argumentos.
 * RAX = Número, RDI = Arg1, RSI = Arg2
 */
static inline uint64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2) 
{
    uint64_t ret;
    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2) // "S" força o uso do registo RSI
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 3 argumentos (Pronta para o seu futuro sys_write).
 * RAX = Número, RDI = Arg1, RSI = Arg2, RDX = Arg3
 */
static inline uint64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) 
{
    uint64_t ret;
    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3) // "d" força o uso do registo RDX
        : "rcx", "r11", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 4 argumentos.
 * RAX = Número, RDI = Arg1, RSI = Arg2, RDX = Arg3, R10 = Arg4
 */
static inline uint64_t syscall4(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    uint64_t ret;
    __asm__ __volatile__(
        "movq %5, %%r10\n\t"   /* Move o 4º argumento para R10 antes do disparo */
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(arg4)
        : "rcx", "r11", "r10", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 5 argumentos.
 * RAX = Número, RDI = Arg1, RSI = Arg2, RDX = Arg3, R10 = Arg4, R8 = Arg5
 */
static inline uint64_t syscall5(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    uint64_t ret;
    __asm__ __volatile__(
        "movq %5, %%r10\n\t"   /* 4º argumento em R10 */
        "movq %6, %%r8\n\t"    /* 5º argumento em R8  */
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(arg4), "r"(arg5)
        : "rcx", "r11", "r10", "r8", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 6 argumentos (Pronta para o seu sys_sendto).
 * RAX = Número, RDI = Arg1, RSI = Arg2, RDX = Arg3, R10 = Arg4, R8 = Arg5, R9 = Arg6
 */
static inline uint64_t syscall6(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    uint64_t ret;
    __asm__ __volatile__(
        "movq %5, %%r10\n\t"   /* 4º argumento em R10 */
        "movq %6, %%r8\n\t"    /* 5º argumento em R8  */
        "movq %7, %%r9\n\t"    /* 6º argumento em R9  */
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(arg1), "S"(arg2), "d"(arg3), "r"(arg4), "r"(arg5), "r"(arg6)
        : "rcx", "r11", "r10", "r8", "r9", "memory"
    );
    return ret;
}


#endif /* _USYSCALL_H_ */