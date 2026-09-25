/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: socket.h
 *    Description: Cabeçalho padrão POSIX para o subsistema de Sockets e Rede.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _SOCKET_H
#define _SOCKET_H

#include <stdint.h>
#include <stddef.h>

/* Domínios / Famílias de Protocolos (Protocol Families) */
#define AF_UNIX     1       /* Sockets locais do Unix (IPC) */
#define AF_LOCAL    1       /* Sinónimo POSIX para AF_UNIX */
#define AF_INET     2       /* Protocolos de Internet IPv4 */
#define AF_INET6    10      /* Protocolos de Internet IPv6 */

/* Tipos de Comunicação de Sockets */
#define SOCK_STREAM 1       /* Fluxo bidirecional e fiável (TCP) */
#define SOCK_DGRAM  2       /* Datagramas não fiáveis de tamanho fixo (UDP) */
#define SOCK_RAW    3       /* Acesso direto aos protocolos de rede (IP cru) */

/* Tipo primitivo para o comprimento de endereços */
typedef unsigned int socklen_t;

/* Estrutura de endereço genérico POSIX */
struct sockaddr {
    unsigned short sa_family;   /* Família do endereço (AF_*) */
    char           sa_data[14]; /* Dados do endereço bruto */
};

#ifdef __cplusplus
extern "C" {
#endif

/* Assinaturas das Syscalls do Subsistema de Rede */
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);

int shutdown(int sockfd, int how);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SOCKET_H */