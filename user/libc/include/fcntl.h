/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: fcntl.h
 *    Description: Cabeçalho padrão POSIX para operações de controlo e flags
 *                 de abertura de ficheiros (File Control Options).
 * 
 *        Author:  Nelson Cole
 *   Created Date: 25/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/types.h>

/* Flags de Acesso ao Ficheiro (Máscaras básicas para o VFS) */
#define O_RDONLY    0x0000    /* Apenas Leitura */
#define O_WRONLY    0x0001    /* Apenas Escrita */
#define O_RDWR      0x0002    /* Leitura e Escrita */
#define O_ACCMODE   0x0003    /* Máscara para modo de acesso */

/* Flags de Estado e Criação (Usadas pelo sh.c e sshd.c) */
#define O_CREAT     0x0040    /* Cria o ficheiro se não existir */
#define O_EXCL      0x0080    /* Erro se O_CREAT e o ficheiro já existir */
#define O_TRUNC     0x0200    /* Trunca o ficheiro para 0 bytes se existir */
#define O_APPEND    0x0400    /* Posiciona o ponteiro de escrita no fim */
#define O_NONBLOCK  0x0800    /* E/S não-bloqueante (Essencial para Sockets TLS) */

/* Comandos da Chamada de Sistema fcntl() */
#define F_DUPFD     0         /* Duplica o descritor de ficheiro */
#define F_GETFD     1         /* Obtém as flags do descritor */
#define F_SETFD     2         /* Define as flags do descritor */
#define F_GETFL     3         /* Obtém as flags de estado do ficheiro */
#define F_SETFL     4         /* Define as flags de estado do ficheiro (ex: O_NONBLOCK) */

/* Flag para o comando F_DUPFD (Fechar ao executar execve) */
#define FD_CLOEXEC  1

#ifdef __cplusplus
extern "C" {
#endif

/* Assinatura da chamada de sistema regulamentar POSIX */
int open(const char *pathname, int flags, ...);
int creat(const char *pathname, mode_t mode);
int fcntl(int fd, int cmd, ...);

#ifdef __cplusplus
}
#endif

#endif /* _FCNTL_H */