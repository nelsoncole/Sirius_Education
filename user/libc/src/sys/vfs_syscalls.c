/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_syscalls.c
 *    Description: Implementação inline e direta das chamadas de sistema (VFS)
 *                 exigidas pelo padrão POSIX. Evita o overhead de saltos de
 *                 função externa utilizando as macros universais usyscall.h.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/usyscall.h>
#include <string.h>
#include <stdarg.h>
#include <string.h>


#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 4096
#endif

static int _resolve_absolute_path(char* out_buf, const char* pathname) {
    if (!pathname || !out_buf) return -1;

    memset(out_buf, 0, MAX_PATH_LEN);

    // CASO 1: O caminho já é absoluto (Começa com '/')
    if (pathname[0] == '/') {
        strncpy(out_buf, pathname, MAX_PATH_LEN - 1);
    } 
    // CASO 2: O caminho é RELATIVO
    else {
        if (getcwd(out_buf, MAX_PATH_LEN) == NULL) {
            return -1; // Falha se não conseguir obter o PWD atual do processo
        }

        size_t pwd_len = strlen(out_buf);

        // Adiciona a barra '/' se o PWD não terminar com uma
        if (pwd_len > 0 && out_buf[pwd_len - 1] != '/') {
            strcat(out_buf, "/");
        }

        // Concatena o resto do caminho relativo fornecido pelo utilizador
        // Usamos strncat para garantir que não ultrapassamos o limite físico do buffer
        strncat(out_buf, pathname, MAX_PATH_LEN - strlen(out_buf) - 1);
    }

    return 0; // Sucesso
}

/*
 * ============================================================================
 * OPERAÇÕES FUNDAMENTAIS EM FILE DESCRIPTORS
 * ============================================================================
 */

int open(const char *pathname, int flags, ...) 
{
    if (!pathname) return -1;

    char absolute_path[MAX_PATH_LEN];
    if (_resolve_absolute_path(absolute_path, pathname) < 0) {
        return -1;
    }
    return (int)syscall2(SYS_OPEN, (uint64_t)absolute_path, (uint64_t)flags);
}

int creat(const char *pathname, mode_t mode) {
    // No padrão POSIX adoptado no Sirius, creat é um atalho para open()
    // com flags de criação, escrita exclusiva e truncagem.
    return open(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

int fcntl(int fd, int cmd, ...) {
    va_list args;
    va_start(args, cmd);

    // Extrai o terceiro argumento como 'long' para cobrir tanto inteiros
    // (como flags O_NONBLOCK) quanto ponteiros na arquitetura x86_64.
    long arg = va_arg(args, long);
    va_end(args);

    // Invoca a stub física do assembly mapeada na unistd.h do Sirius
    return (int)syscall3(SYS_FCNTL, (uint64_t)fd, (uint64_t)cmd, (uint64_t)arg);
}

ssize_t read(int fd, void *buf, size_t count) 
{
    return (ssize_t)syscall3(SYS_READ, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

ssize_t write(int fd, const void *buf, size_t count) 
{
    return (ssize_t)syscall3(SYS_WRITE, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
}

int close(int fd) 
{
    return (int)syscall1(SYS_CLOSE, (uint64_t)fd);
}

int dup(int oldfd) 
{
    /* 
     * Nota: Como a tua tabela mapeia estritamente o SYS_DUP2, implementamos o dup(oldfd) 
     * clássico forçando o Kernel a escolher o próximo FD livre ou chamando a SYS_DUP2.
     * Caso o teu kernel não tenha a SYS_DUP nativa, podes usar a SYS_DUP2 com -1.
     */
    return (int)syscall2(SYS_DUP2, (uint64_t)oldfd, (uint64_t)-1);
}

int dup2(int oldfd, int newfd) 
{
    return (int)syscall2(SYS_DUP2, (uint64_t)oldfd, (uint64_t)newfd);
}

off_t lseek(int fd, off_t offset, int whence) 
{
    return (off_t)syscall3(SYS_SEEK, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence);
}

int pipe(int pipefd[2]) 
{
    return (int)syscall1(SYS_IOCTL, (uint64_t)pipefd);
}

/*
 * ============================================================================
 * GESTÃO DE SISTEMA DE FICHEIROS
 * ============================================================================
 */
int unlink(const char *pathname) 
{
    if (!pathname) return -1;

    char absolute_path[MAX_PATH_LEN];
    if (_resolve_absolute_path(absolute_path, pathname) < 0) {
        return -1;
    }

    return (int)syscall1(SYS_UNLINK, (uint64_t)absolute_path);
}

int rmdir(const char *pathname) 
{
    if (!pathname) return -1;

    char absolute_path[MAX_PATH_LEN];
    if (_resolve_absolute_path(absolute_path, pathname) < 0) {
        return -1;
    }

    return (int)syscall1(SYS_RMDIR, (uint64_t)absolute_path);
}

int access(const char *pathname, int mode) 
{
    if (!pathname) return -1;

    char absolute_path[MAX_PATH_LEN];
    if (_resolve_absolute_path(absolute_path, pathname) < 0) {
        return -1;
    }

    return (int)syscall2(SYS_STAT, (uint64_t)absolute_path, (uint64_t)mode);
}

int isatty(int fd) 
{
    /* 
     * O truque vital do Unix: Envia um comando de teste nulo via ioctl.
     * Se o Kernel responder com sucesso (0), significa que o FD pertence a um 
     * terminal legítimo (como as tuas TTYs ou as tuas novas PTYs /dev/pts/X).
     */
    return (syscall2(SYS_IOCTL, (uint64_t)fd, 0) == 0) ? 1 : 0;
}

int stat(const char *pathname, struct stat *statbuf)
{
    if (!pathname || !statbuf) {
        return -1; /* Retorna erro de argumento inválido (EINVAL / EFAULT) */
    }

    char absolute_path[MAX_PATH_LEN];
    if (_resolve_absolute_path(absolute_path, pathname) < 0) {
        return -1; /* Falha ao tentar resolver o PWD do processo */
    }

    return (int)syscall2(SYS_STAT, (uint64_t)absolute_path, (uint64_t)statbuf);
}



int fstat(int fd, struct stat *statbuf)
{
    if (fd < 0 || !statbuf) {
        return -1;
    }

    return (int)syscall2(SYS_FSTAT, (uint64_t)fd, (uint64_t)statbuf);
}
