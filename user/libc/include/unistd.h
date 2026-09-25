/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: unistd.h
 *    Description: Cabeçalho padrão POSIX para constantes, tipos e declarações
 *                 de chamadas de sistema fundamentais (E/S de ficheiros,
 *                 controlo de processos e pipes).
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _UNISTD_H_
#define _UNISTD_H_

#include <sys/types.h>
#include <stddef.h>

/* Constantes para os File Descriptors padrão do padrão POSIX */
#define STDIN_FILENO  0  /* Entrada padrão (Teclado / input da PTY) */
#define STDOUT_FILENO 1  /* Saída padrão (Ecrã / tty0 / output da PTY) */
#define STDERR_FILENO 2  /* Saída de erro padrão (Ecrã / tty0) */

/* Constantes para posicionamento de ponteiro de arquivo (lseek) */
#ifndef SEEK_SET
#define SEEK_SET      0  /* Posiciona a partir do início do ficheiro */
#define SEEK_CUR      1  /* Posiciona a partir da posição atual */
#define SEEK_END      2  /* Posiciona a partir do fim do ficheiro */
#endif

/* Constantes para validação de acesso a ficheiros (access) */
#define F_OK          0  /* Teste de existência do ficheiro */
#define X_OK          1  /* Teste de permissão de execução */
#define W_OK          2  /* Teste de permissão de escrita */
#define R_OK          4  /* Teste de permissão de leitura */

/* 
 * ============================================================================
 * ASSINATURAS DAS FUNÇÕES POSIX (Interface da LibC)
 * ============================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Operações fundamentais em File Descriptors (VFS) */
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int     close(int fd);
int     dup(int oldfd);
int     dup2(int oldfd, int newfd);
off_t   lseek(int fd, off_t offset, int whence);
int     pipe(int pipefd[2]);

/* Gestão de Sistema de Ficheiros e Diretórios (VFS) */
int     chdir(const char *path);
char   *getcwd(char *buf, size_t size);
int     unlink(const char *pathname);
int     rmdir(const char *pathname);
int     access(const char *pathname, int mode);
int     isatty(int fd);

/* Gestão de Processos e Fluxo de Execução */
pid_t   fork(void);
int     execve(const char *pathname, char *const argv[], char *const envp[]);
int     execv(const char *pathname, char *const argv[]);
int     execl(const char *pathname, const char *arg, ...);
void    _exit(int status);

/* Identificação e Utilidades de Processos */
pid_t   getpid(void);
pid_t   getppid(void);
uid_t   getuid(void);
gid_t   getgid(void);
int     setuid(uid_t uid);
int     setgid(gid_t gid);

/* Utilitários de Tempo e Sincronismo */
unsigned int sleep(unsigned int seconds);
int     usleep(unsigned int usec);

#ifdef __cplusplus
}
#endif

#endif /* _UNISTD_H_ */