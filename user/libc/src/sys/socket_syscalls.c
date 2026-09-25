/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: socket_syscalls.c
 *    Description: Wrappers diretos e inline para o subsistema de Sockets,
 *                 conectando Ring 3 ao subsistema de rede do Kernel Core.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * ============================================================================
 */

#include <sys/socket.h>
#include <sys/usyscall.h>
#include <stddef.h>

int socket(int domain, int type, int protocol) 
{
    return (int)syscall3(SYS_SOCKET, (uint64_t)domain, (uint64_t)type, (uint64_t)protocol);
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) 
{
    return (int)syscall3(SYS_BIND, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen);
}

int listen(int sockfd, int backlog) 
{
    return (int)syscall2(SYS_LISTEN, (uint64_t)sockfd, (uint64_t)backlog);
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) 
{
    return (int)syscall3(SYS_ACCEPT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen);
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) 
{
    return (int)syscall3(SYS_CONNECT, (uint64_t)sockfd, (uint64_t)addr, (uint64_t)addrlen);
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) 
{
    return (ssize_t)syscall4(SYS_SEND, (uint64_t)sockfd, (uint64_t)buf, (uint64_t)len, (uint64_t)flags);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) 
{
    return (ssize_t)syscall4(SYS_RECV, (uint64_t)sockfd, (uint64_t)buf, (uint64_t)len, (uint64_t)flags);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen) 
{
    /* Usa a macro inline de 6 argumentos que preparaste no teu usyscall.h! */
    return (ssize_t)syscall6(SYS_SENDTO, (uint64_t)sockfd, (uint64_t)buf, (uint64_t)len, 
                             (uint64_t)flags, (uint64_t)dest_addr, (uint64_t)addrlen);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen) 
{
    return (ssize_t)syscall6(SYS_RECVFROM, (uint64_t)sockfd, (uint64_t)buf, (uint64_t)len, 
                             (uint64_t)flags, (uint64_t)src_addr, (uint64_t)addrlen);
}

int shutdown(int sockfd, int how) 
{
    return (int)syscall2(SYS_SHUTDOWN, (uint64_t)sockfd, (uint64_t)how);
}