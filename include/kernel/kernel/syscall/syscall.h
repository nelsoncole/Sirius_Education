/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: syscall.h
 *    Description: Cabeçalho da Camada de Abstração de Chamadas de Sistema (SCI).
 *                 Define a tabela de vetores (sys_call_table), os números
 *                 de identificação dos serviços e os protótipos de Ring 0.
 * 
 *         Author: Nelson Cole
 *   Created Date: 05/09/2026
 * 
 *    Modified By: Nelson Cole / AI Collaborator
 *  Modified Date: 15/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <kernel/lib/stdint.h>
#include <kernel/fs/vfs/vfs.h> // Importante para reconhecer o tipo vfs_stat_t

/*
 * CONFIGURAÇÃO DOS NÚMEROS DE CHAMADA DE SISTEMA (SYSCALL NUMBERS)
 * ------------------------------------------------------------------------
 * Índices lógicos passados no registador RAX pelas aplicações em Ring 3.
 */
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

/* Subsistema de modulo do kernel */
#define SYS_KMOD_LOAD   38
#define SYS_KMOD_UNLOAD 39
#define SYS_KMOD_PRINT  40

/* Número total de chamadas suportadas nesta fase com suporte de Rede Completo */
#define MAX_SYSCALLS 41


/**
 * Inicializa e programa os registadores de hardware MSR (STAR, LSTAR, FMASK)
 * locais do núcleo atual para ativar o suporte à instrução 'syscall'.
 * Deve ser executada individualmente pelo BSP e por cada AP no arranque.
 */
void syscall_init(void);

/**
 * Manipulador mestre em C (SCI Dispatcher). Recebe o fluxo do Stub em 
 * Assembly, valida o índice contido em RAX e despacha para a função correta.
 * 
 * @param syscall_num O ID do serviço (vindo de RAX mapeado para RDI).
 * @param arg1 Primeiro argumento da chamada (vindo de RDI mapeado para RSI).
 * @param arg2 Segundo argumento da chamada (vindo de RSI mapeado para RDX).
 * @param arg3 Terceiro argumento da chamada (vindo de RDX mapeado para RCX).
 * @return O valor de retorno da operation que será devolvido à aplicação em RAX.
 */
uint64_t syscall_dispatcher(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

/*
 * ============================================================================
 * PROTÓTIPOS DOS SERVIÇOS NATIVOS INTERNOS DO KERNEL (HANDLERS)
 * ============================================================================
 */

/* Operações de Gestão e Montagem de Volumes */
uint64_t sys_mount(const char* device_name, const char* mount_path, const char* fs_type);
uint64_t sys_umount(const char* mount_path);

/* Operações de Ficheiros Baseadas em Descritores Lógicos (fd) - Corrigidas */
uint64_t sys_open(const char* path, uint32_t flags);
uint64_t sys_close(int fd);
uint64_t sys_read(int fd, void* buffer, uint32_t size);
uint64_t sys_write(int fd, const void* buffer, uint32_t size);
uint64_t sys_seek(int fd, int64_t offset, int whence);
uint64_t sys_flush(int fd);

/* Operações Avançadas de Metadados e Remoção por Caminho */
uint64_t sys_stat(const char* path, vfs_stat_t* buf);
uint64_t sys_chmod(const char* path, uint16_t mode);
uint64_t sys_unlink(const char* path);
uint64_t sys_rmdir(const char* path);
uint64_t sys_rename(const char* old_path, const char* new_name);

/* Operações Primitivas do Processo e Alocação */
uint64_t sys_brk(void* addr);
uint64_t sys_exit(uint64_t code);

uint64_t sys_ioctl(int fd, unsigned long request, void *arg);
uint64_t sys_fork(void);
uint64_t sys_execve(const char *pathname, char *const argv[], char *const envp[]);
uint64_t sys_mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset);
uint64_t sys_munmap(void *addr, size_t length);
uint64_t sys_getpid(void);
uint64_t sys_getppid(void);
uint64_t sys_waitpid(int32_t pid, int *wstatus, int options);
uint64_t sys_sleep(unsigned int seconds);
uint64_t sys_kill(int32_t pid, int sig);
uint64_t sys_sigaction(int signum, const void *act, void *oldact);
uint64_t sys_socket(int domain, int type, int protocol);
uint64_t sys_bind(int sockfd, const void *addr, uint32_t addrlen);
uint64_t sys_listen(int sockfd, int backlog);
uint64_t sys_accept(int sockfd, void *addr, uint32_t *addrlen);
uint64_t sys_connect(int sockfd, const void *addr, uint32_t addrlen);
uint64_t sys_send(int sockfd, const void *buf, size_t len, int flags);
uint64_t sys_recv(int sockfd, void *buf, size_t len, int flags);
uint64_t sys_sendto(int sockfd, const void* buf, size_t len, int flags, const void* dest_addr, uint64_t addrlen);
uint64_t sys_recvfrom(int sockfd, void* buf, size_t len, int flags, void* src_addr, uint64_t* addrlen);
uint64_t sys_shutdown(int sockfd, int how);
uint64_t sys_setsockopt(int sockfd, int level, int optname, const void *optval, uint32_t optlen);
uint64_t sys_getsockopt(int sockfd, int level, int optname, void *optval, uint32_t *optlen);

/* Operações de Módulos */
uint64_t sys_kmod_load(const uint8_t *user_buffer, size_t size);
uint64_t sys_kmod_unload(const char *user_name);
uint64_t sys_kmod_print(void);

#endif /* _SYSCALL_H_ */