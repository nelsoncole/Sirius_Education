/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sys/un.h
 *    Description: Cabeçalho POSIX para endereçamento de sockets locais.
 *                 Define a estrutura sockaddr_un para comunicação entre
 *                 processos no mesmo sistema.
 *
 *        Author:  Nelson Cole
 *   Created Date: 30/10/2026
 *
 *        License: MIT
 * ============================================================================
 */

#ifndef _UN_H_
#define _UN_H_

#include <stdint.h>

#ifndef AF_UNIX
#define AF_UNIX  1
#endif

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif

#define UNIX_PATH_MAX 108

struct sockaddr_un {
    uint16_t sun_family;
    char sun_path[UNIX_PATH_MAX];
};

#endif