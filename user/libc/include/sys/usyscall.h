/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: usyscall.h
 *    Description: Invólucros universais em Assembly Inline para a instrução
 *                 nativa 'syscall' em ambiente x86_64 (Ring 3).
 *                 Respeita a System V AMD64 ABI e protege RCX/R11.
 *                 Totalmente otimizado e seguro para compilação em -O2.
 *
 *         Author: Nelson Cole
 *   Created Date: 17/09/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 25/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#ifndef _USYSCALL_H_
#define _USYSCALL_H_

#include <stdint.h>

enum {
    /* Operações Básicas de I/O, Memória e Execução */
    SYS_READ = 0,
    SYS_WRITE,
    SYS_BRK,
    SYS_EXIT,

    /* Expansão das operações do Sistema de Ficheiros Virtual (VFS) */
    SYS_MOUNT,
    SYS_UMOUNT,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_SEEK,
    SYS_FLUSH,
    SYS_STAT,
    SYS_CHMOD,
    SYS_UNLINK,
    SYS_RMDIR,
    SYS_RENAME,
    SYS_MKDIR,
    SYS_GETDENTS,
    SYS_DUP2,
    SYS_IOCTL,

    /* Gestão de Processos e Memória Avançada */
    SYS_FORK,
    SYS_EXECVE,
    SYS_MMAP,
    SYS_MUNMAP,
    SYS_GETPID,
    SYS_GETPPID,

    /* Sincronização, Tempo e Sinais */
    SYS_WAITPID,
    SYS_SLEEP,
    SYS_KILL,
    SYS_SIGACTION,

    /* Subsistema de Sockets e Rede */
    SYS_SOCKET,
    SYS_BIND,
    SYS_LISTEN,
    SYS_ACCEPT,
    SYS_CONNECT,
    SYS_SEND,
    SYS_RECV,
    SYS_SENDTO,
    SYS_RECVFROM,
    SYS_SHUTDOWN,
    SYS_SETSOCKOPT,
    SYS_GETSOCKOPT,

    /* Subsistema de módulo do kernel */
    SYS_KMOD_LOAD,
    SYS_KMOD_UNLOAD,
    SYS_KMOD_PRINT,

    /* O compilador define automaticamente MAX_SYSCALLS com o valor total correto (43) */
    MAX_SYSCALLS
};

/**
 * @brief Syscall com 0 argumentos.
 * RAX = Número da Syscall
 */
__attribute__((optimize("O0")))
static inline uint64_t syscall0(uint64_t num) 
{
    uint64_t ret;
    register uint64_t _num asm("rax") = num;

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "r"(_num)
        : "rcx", "r11", "cc", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 1 argumento (Usada pela sbrk_user).
 * RAX = Número da Syscall, RDI = Argumento 1
 */
__attribute__((optimize("O0")))
static inline uint64_t syscall1(uint64_t num, uint64_t arg1) 
{
    uint64_t ret;
    register uint64_t _num asm("rax") = num;
    register uint64_t _a1  asm("rdi") = arg1;

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "r"(_num), "r"(_a1)
        : "rcx", "r11", "cc", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 2 argumentos.
 * RAX = Número, RDI = Arg1, RSI = Arg2
 */
__attribute__((optimize("O0")))
static inline uint64_t syscall2(uint64_t num, uint64_t arg1, uint64_t arg2) 
{
    uint64_t ret;
    register uint64_t _num asm("rax") = num;
    register uint64_t _a1  asm("rdi") = arg1;
    register uint64_t _a2  asm("rsi") = arg2;

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "r"(_num), "r"(_a1), "r"(_a2)
        : "rcx", "r11", "cc", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 3 argumentos (Usada pelo sys_getdents e sys_ioctl).
 * RAX = Número, RDI = Arg1, RSI = Arg2, RDX = Arg3
 */
__attribute__((optimize("O0")))
static inline uint64_t syscall3(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) 
{
    uint64_t ret;
    register uint64_t _num asm("rax") = num;
    register uint64_t _a1  asm("rdi") = arg1;
    register uint64_t _a2  asm("rsi") = arg2;
    register uint64_t _a3  asm("rdx") = arg3;

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "r"(_num), "r"(_a1), "r"(_a2), "r"(_a3)
        : "rcx", "r11", "cc", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 4 argumentos.
 * RAX = Número, RDI = Arg1, RSI = Arg2, RDX = Arg3, R10 = Arg4
 */
__attribute__((optimize("O0")))
static inline uint64_t syscall4(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    uint64_t ret;
    register uint64_t _num asm("rax") = num;
    register uint64_t _a1  asm("rdi") = arg1;
    register uint64_t _a2  asm("rsi") = arg2;
    register uint64_t _a3  asm("rdx") = arg3;
    register uint64_t _a4  asm("r10") = arg4; // Vinculação explícita ao R10 antes do ASM

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "r"(_num), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4)
        : "rcx", "r11", "cc", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 5 argumentos.
 * RAX = Número, RDI = Arg1, RSI = Arg2, RDX = Arg3, R10 = Arg4, R8 = Arg5
 */
__attribute__((optimize("O0")))
static inline uint64_t syscall5(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
    uint64_t ret;
    register uint64_t _num asm("rax") = num;
    register uint64_t _a1  asm("rdi") = arg1;
    register uint64_t _a2  asm("rsi") = arg2;
    register uint64_t _a3  asm("rdx") = arg3;
    register uint64_t _a4  asm("r10") = arg4;
    register uint64_t _a5  asm("r8")  = arg5; // Vinculação explícita ao R8

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "r"(_num), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5)
        : "rcx", "r11", "cc", "memory"
    );
    return ret;
}

/**
 * @brief Syscall com 6 argumentos.
 * RAX = Número, RDI = Arg1, RSI = Arg2, RDX = Arg3, R10 = Arg4, R8 = Arg5, R9 = Arg6
 */
__attribute__((optimize("O0")))
static inline uint64_t syscall6(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
    uint64_t ret;
    register uint64_t _num asm("rax") = num;
    register uint64_t _a1  asm("rdi") = arg1;
    register uint64_t _a2  asm("rsi") = arg2;
    register uint64_t _a3  asm("rdx") = arg3;
    register uint64_t _a4  asm("r10") = arg4;
    register uint64_t _a5  asm("r8")  = arg5;
    register uint64_t _a6  asm("r9")  = arg6; // Vinculação explícita ao R9

    __asm__ __volatile__(
        "syscall"
        : "=a"(ret)
        : "r"(_num), "r"(_a1), "r"(_a2), "r"(_a3), "r"(_a4), "r"(_a5), "r"(_a6)
        : "rcx", "r11", "cc", "memory"
    );
    return ret;
}

#endif /* _USYSCALL_H_ */