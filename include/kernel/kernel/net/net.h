/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: net.h
 *    Description: Estruturas binárias nativas e definições base da pilha de
 *                 protocolos de rede da Internet (IPv4, UDP, TCP e Endianness).
 * 
 *         Author: Nelson Cole
 *   Created Date: 18/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 20/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _NET_H_
#define _NET_H_

#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>
#include <kernel/lib/stdio.h>

/* EtherTypes Clássicos da Camada Ethernet */
#define ETH_P_IP   0x0800  /* Internet Protocol packet */
#define ETH_P_ARP  0x0806  /* Address Resolution packet */

#define DHCP_BOOTREQUEST 1
#define DHCP_BOOTREPLY   2

/* Identificadores de Protocolos de Transporte IP (Cabeçalho IP) */
#define IPPROTO_ICMP 1
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

/* Definições de Estados Canónicos da Máquina de Estados TCP */
#define TCP_STATE_CLOSED      0
#define TCP_STATE_SYN_SENT    1
#define TCP_STATE_SYN_RECV    2
#define TCP_STATE_ESTABLISHED 3

/**
 * @brief Estrutura clássica de endereço de socket da Internet (IPv4).
 * Conforme o padrão POSIX para comunicação de rede via AF_INET.
 */
struct in_addr {
    uint32_t s_addr;
};
struct sockaddr_in {
    uint16_t sin_family;   /* Família do endereço: Sempre AF_INET */
    uint16_t sin_port;     /* Porta de transporte (Network Byte Order) */
    struct in_addr sin_addr;     /* Endereço IPv4 de 32-bits (Network Byte Order) */
    uint8_t  sin_zero[8];  /* Preenchimento de alinhamento com struct sockaddr */
};

/* Cabeçalho Ethernet Físico Clássico (14 bytes) */
typedef struct {
    uint8_t  dest_mac[6];  /* MAC de Destino */
    uint8_t  src_mac[6];   /* MAC de Origem */
    uint16_t ethertype;    /* Tipo do payload (Big-Endian) */
} __attribute__((packed)) ethernet_header_t;

/* Estrutura do Pacote DHCP Padrão (Fixada em 236 bytes de cabeçalho base + opções) */
typedef struct {
    uint8_t  op;           /* Operação: 1 = Request, 2 = Reply */
    uint8_t  htype;        /* Tipo de hardware: 1 = Ethernet */
    uint8_t  hlen;         /* Tamanho do MAC: 6 */
    uint8_t  hops;         /* Padrão: 0 */
    uint32_t xid;          /* Identificador único da transação (Transaction ID) */
    uint16_t secs;         /* Segundos decorridos */
    uint16_t flags;        /* Flags (0x8000 para forçar respostas em Broadcast se necessário) */
    uint32_t ciaddr;       /* Client IP address (IP atual do cliente, se tiver) */
    uint32_t yiaddr;       /* Your IP address (O IP que o servidor te oferece) */
    uint32_t siaddr;       /* Server IP address */
    uint32_t giaddr;       /* Gateway IP address relay */
    uint8_t  chaddr[16];   /* Client Hardware Address (O meu endereço MAC físico) */
    uint8_t  sname[64];    /* Nome do servidor (Opcional) */
    uint8_t  file[128];    /* Arquivo de boot (Opcional) */
    uint32_t magic_cookie; /* Sempre 0x63825363 (Indica início das opções DHCP) */
} __attribute__((packed)) dhcp_header_t;

/* Cabeçalho Binário ARP (28 bytes para mapeamento Ethernet/IPv4) */
typedef struct {
    uint16_t hw_type;      /* Tipo de hardware (1 = Ethernet) */
    uint16_t proto_type;   /* Tipo de protocolo (0x0800 = IPv4) */
    uint8_t  hw_len;       /* Tamanho do endereço físico (6 para MAC) */
    uint8_t  proto_len;    /* Tamanho do endereço lógico (4 para IPv4) */
    uint16_t opcode;       /* Operação (1 = Request, 2 = Reply) */
    uint8_t  src_mac[6];   /* MAC do Remetente */
    uint32_t src_ip;       /* IP do Remetente */
    uint8_t  dest_mac[6];  /* MAC do Destinatário */
    uint32_t dest_ip;      /* IP do Destinatário */
} __attribute__((packed)) arp_header_t;

/**
 * @brief Cabeçalho Padrão IPv4 (Mínimo de 20 bytes).
 */
typedef struct {
    uint8_t  version_ihl;     /* Versão (4 bits) + Comprimento do Cabeçalho IHL (4 bits) */
    uint8_t  tos;             /* Tipo de Serviço (Type of Service) */
    uint16_t total_len;       /* Comprimento total do pacote (Cabeçalho + Dados) */
    uint16_t id;              /* Identificador único do fragmento */
    uint16_t flags_fragment;  /* Sinalizadores (3 bits) + Deslocamento do Fragmento (13 bits) */
    uint8_t  ttl;             /* Tempo de Vida (Time to Live) */
    uint8_t  protocol;        /* Protocolo da Camada de Transporte (TCP=6, UDP=17) */
    uint16_t checksum;        /* Soma de verificação do cabeçalho IP */
    uint32_t src_ip;          /* Endereço IPv4 de Origem */
    uint32_t dest_ip;         /* Endereço IPv4 de Destino */
} __attribute__((packed)) ip_header_t;

/**
 * @brief Cabeçalho do Protocolo User Datagram Protocol (UDP) (8 bytes).
 */
typedef struct {
    uint16_t src_port;        /* Porta de Origem */
    uint16_t dest_port;       /* Porta de Destino */
    uint16_t length;          /* Comprimento total do datagrama (Cabeçalho + Dados) */
    uint16_t checksum;        /* Checksum do UDP (opcional em IPv4, obrigatório em IPv6) */
} __attribute__((packed)) udp_header_t;

/**
 * @brief Cabeçalho do Transmission Control Protocol (TCP) (Mínimo de 20 bytes).
 */
typedef struct {
    uint16_t src_port;        /* Porta de Origem */
    uint16_t dest_port;       /* Porta de Destino */
    uint32_t seq_num;         /* Número de Sequência */
    uint32_t ack_num;         /* Número de Confirmação (Acknowledgment Number) */
    uint16_t data_offset_flags; /* Offset de Dados (4 bits) + Reservado (3 bits) + Flags (9 bits) */
    uint16_t window_size;     /* Janela de Receção (Window Size) */
    uint16_t checksum;        /* Checksum TCP (Calculado com Pseudo-Cabeçalho IP) */
    uint16_t urgent_ptr;      /* Ponteiro Urgente (Urgent Pointer) */
} __attribute__((packed)) tcp_header_t;

/* ============================================================================
 *           Macros Atómicas para Conversão de Endianness
 *           (Garante portabilidade Little-Endian para Big-Endian / Network)
 * ============================================================================ */

#define htons(v) ((((uint16_t)(v) & 0xFF00) >> 8) | (((uint16_t)(v) & 0x00FF) << 8))
#define ntohs(v) htons(v)

#define htonl(v) ((((uint32_t)(v) & 0xFF000000) >> 24) | \
                  (((uint32_t)(v) & 0x00FF0000) >> 8)  | \
                  (((uint32_t)(v) & 0x0000FF00) << 8)  | \
                  (((uint32_t)(v) & 0x000000FF) << 24))
#define ntohl(v) htonl(v)

/**
 * @brief inet_ntoa_r - Converte um endereço IP de 32-bits para string de forma segura (Reentrante).
 * 
 * @param ip_addr O endereço IPv4 em Network Byte Order (Big-Endian).
 * @param buf     Ponteiro para o buffer de destino onde a string será gravada.
 * @param buflen  Tamanho do buffer de destino (deve ter pelo menos 16 bytes).
 * @return Retorna o ponteiro para o início da string em caso de sucesso, ou NULL em falha.
 */
static inline char* inet_ntoa_r(uint32_t ip_addr, char* buf, size_t buflen)
{
    if (!buf || buflen < 16) return NULL;

    /* Desembrulha os 4 octetos diretamente a partir do layout binário na memória */
    uint8_t *bytes = (uint8_t*)&ip_addr;

    /* 
     * Monta a string no formato "A.B.C.D".
     * Se o seu Kernel já possui a função ksprintf ou ksnprintf, utilize-a aqui:
     */
    ksprintf(buf, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);

    return buf;
}

/**
 * @brief inet_ntoa - Versão clássica POSIX. Utiliza um buffer estático local.
 *                    Nota: Não é reentrante em ambiente SMP rigoroso se múltiplos 
 *                    núcleos chamarem ao mesmo tempo, mas é ideal para kprintf simples.
 */
static inline char* inet_ntoa(uint32_t ip_addr)
{
    static char static_buf[16]; /* Espaço exato para "255.255.255.255\0" */
    return inet_ntoa_r(ip_addr, static_buf, sizeof(static_buf));
}

int net_driver_register(const uint8_t *mac_addr, void* ops_table);
int net_driver_transmit(const void* buffer, uint32_t packet_size);
int net_driver_receive(const void* buffer, uint32_t packet_size);
void network_rx_thread(void);
void net_init(void);
int net_get_interface_mac(uint32_t interface_id, uint8_t* mac_out);
int net_set_interface_ip(uint32_t ip);
int net_set_interface_mask(uint32_t mask);
int net_set_interface_gateway(uint32_t gateway);
int net_get_interface_ip(uint32_t interface_id, uint32_t* ip_out);
int net_get_interface_mask(uint32_t interface_id, uint32_t* mask_out);
int net_get_interface_gateway(uint32_t interface_id, uint32_t* gateway_out);
int net_get_interface_init(uint32_t interface_id, bool flag);
int net_get_interface_is_online(uint32_t interface_id, bool flag);

#endif /* _NET_H_ */