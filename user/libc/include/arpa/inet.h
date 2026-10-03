#ifndef __INET_H__
#define __INET_H__

#include <netinet/in.h>

/*
 * ============================================================================
 * MACROS DE CONVERSÃO DE ENDIANNESS (Host to Network / Network to Host)
 * No x86_64 (Little-Endian) para Rede (Big-Endian).
 * ============================================================================
 */

/* Host to Network Short (16 bits) */
#define htons(v) ((((uint16_t)(v) & 0xFF00) >> 8) | \
                  (((uint16_t)(v) & 0x00FF) << 8))

/* Host to Network Long (32 bits) */
#define htonl(v) ((((uint32_t)(v) & 0xFF000000) >> 24) | \
                  (((uint32_t)(v) & 0x00FF0000) >> 8)  | \
                  (((uint32_t)(v) & 0x0000FF00) << 8)  | \
                  (((uint32_t)(v) & 0x000000FF) << 24))

/* Network to Host Short (16 bits) */
#define ntohs(v) htons(v)

/* Network to Host Long (32 bits) */
#define ntohl(v) htonl(v)


in_addr_t	inet_addr(const char *cp);
in_addr_t	inet_lnaof(struct in_addr in);
struct in_addr	inet_makeaddr(in_addr_t net, in_addr_t lna);
in_addr_t	inet_netof(struct in_addr in);
in_addr_t	inet_network(const char *cp);
char	*inet_ntoa(struct in_addr in);
int inet_pton(int af, const char *src, void *dst);

#endif
