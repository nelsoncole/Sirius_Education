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
#include <sys/usyscall.h>  /* Cabeçalho com as macros inline syscall0-syscall6 */

/*
 * ============================================================================
 * OPERAÇÕES FUNDAMENTAIS EM FILE DESCRIPTORS
 * ============================================================================
 */

int open(const char *pathname, int flags, ...) 
{
    /* 
     * Encapsula a chamada utilizando a macro de 2 argumentos do teu usyscall.h.
     * Passa o ponteiro da string do caminho e a máscara binária de flags.
     */
    return (int)syscall2(SYS_OPEN, (uint64_t)pathname, (uint64_t)flags);
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
    return (int)syscall1(SYS_IOCTL, (uint64_t)pipefd); /* Ajusta se tiveres SYS_PIPE dedicada */
}

/*
 * ============================================================================
 * GESTÃO DE SISTEMA DE FICHEIROS E DIRETÓRIOS
 * ============================================================================
 */

int chdir(const char *path) 
{
    /* Usa a operação polimórica do VFS Core mapeada no teu Kernel */
    return (int)syscall1(SYS_IOCTL, (uint64_t)path); 
}

char *getcwd(char *buf, size_t size) 
{
    long ret = (long)syscall2(SYS_IOCTL, (uint64_t)buf, (uint64_t)size);
    return (ret < 0) ? NULL : buf;
}

int unlink(const char *pathname) 
{
    return (int)syscall1(SYS_UNLINK, (uint64_t)pathname);
}

int rmdir(const char *pathname) 
{
    return (int)syscall1(SYS_RMDIR, (uint64_t)pathname);
}

int access(const char *pathname, int mode) 
{
    /* Reaproveita o SYS_STAT ou a tua tabela interna para validar as permissões */
    return (int)syscall2(SYS_STAT, (uint64_t)pathname, (uint64_t)mode);
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