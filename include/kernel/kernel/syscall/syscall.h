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
 *  Modified Date: 13/09/2026
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

/* Número total de chamadas suportadas nesta fase com suporte VFS */
#define MAX_SYSCALLS 15

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

#endif /* _SYSCALL_H_ */