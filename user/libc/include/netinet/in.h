/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: in.h
 *    Description: Cabeçalho padrão POSIX para a família de protocolos da 
 *                 Internet (IPv4/IPv6). Define estruturas de endereçamento
 *                 e macros para conversão de Endianness (Byte Order).
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _IN_H_
#define _IN_H_

#include <stdint.h>

/* Família de Endereços de Internet (Mapeado em sys/socket.h) */
#ifndef AF_INET
#define AF_INET     2       /* Protocolos de Internet IPv4 */
#endif

/* Protocolos IP Padrão (IPPROTO_*) */
#define IPPROTO_IP   0       /* Protocolo IP Dummy/Padrão */
#define IPPROTO_ICMP 1       /* Internet Control Message Protocol */
#define IPPROTO_TCP  6       /* Transmission Control Protocol */
#define IPPROTO_UDP  17      /* User Datagram Protocol */
#define IPPROTO_RAW  255     /* Protocolo de acesso Raw */

/* Tipo primitivo para portas de rede e endereços IP (Ordem de bytes de rede) */
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;

/* Endereço IPv4 de 32 bits em Network Byte Order (Big-Endian) */
struct in_addr {
    in_addr_t s_addr;
};

/* Estrutura de endereço IPv4 (sockaddr_in) exigida pelo padrão POSIX */
struct sockaddr_in {
    uint16_t       sin_family;   /* Família do endereço: Sempre AF_INET */
    in_port_t      sin_port;     /* Porta de transporte (Network Byte Order) */
    struct in_addr sin_addr;     /* Endereço IPv4 (Network Byte Order) */
    unsigned char  sin_zero[8];  /* Preenchimento para alinhar com struct sockaddr */
};

/* Endereços IPv4 Especiais de Controle */
#define INADDR_ANY       ((in_addr_t) 0x00000000) /* Escuta em todas as interfaces (0.0.0.0) */
#define INADDR_LOOPBACK  ((in_addr_t) 0x7f000001) /* Interface local (127.0.0.1) */
#define INADDR_BROADCAST ((in_addr_t) 0xffffffff) /* Difusão global (255.255.255.255) */
#define INADDR_NONE      ((in_addr_t) 0xffffffff) /* 255.255.255.255 (Endereço Inválido / Erro) */

#endif /* _IN_H_ */