/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: dhcp.c
 *    Description: Cliente DHCP nativo em Kernel. Forja requisições de boot
 * 
 *         Author: Nelson Cole
 *   Created Date: 18/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 18/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/net.h>
#include <kernel/kernel/net/socket.h>
#include <kernel/klib.h>
#include <kernel/lib/string.h>

#define DHCP_DISCOVER_XID 0x39A34125 /* ID de transação arbitrário fixo para validação */

/* Protótipo da camada de transporte UDP que criámos */
extern long udp_send_datagram(socket_t* sock, const void* buf, unsigned long len, struct sockaddr_in* dest);

/**
 * @brief dhcp_send_discover - Forja e dispara o pacote inicial DHCP Discover em Broadcast.
 */
int dhcp_send_discover(socket_t* sock)
{
    if (!sock) return -1;

    /* Aloca espaço para o cabeçalho base + 4 bytes para a opção de término */
    uint32_t payload_size = sizeof(dhcp_header_t) + 4;
    uint8_t* buffer = (uint8_t*)kmalloc(payload_size);
    if (!buffer) return -2;
    memset(buffer, 0, payload_size);

    /* 1. Preenche o cabeçalho binário canónico do DHCP */
    dhcp_header_t* dhcp = (dhcp_header_t*)buffer;
    dhcp->op           = DHCP_BOOTREQUEST;
    dhcp->htype        = 1; /* Ethernet */
    dhcp->hlen         = 6; /* MAC de 6 bytes */
    dhcp->xid          = htonl(DHCP_DISCOVER_XID);
    dhcp->flags        = htons(0x8000); /* Solicita resposta em Broadcast (QEMU amigável) */
    
    /* Copia o MAC físico para os bytes iniciais do campo chaddr de 16 bytes */
    memcpy(dhcp->chaddr, g_net_interface_mac, 6);
    
    /* Magic Cookie obrigatório antes das opções (99.130.83.99) */
    dhcp->magic_cookie = htonl(0x63825363);

    /* 2. Adiciona Opções DHCP (Formato TLV: Type-Length-Value) */
    uint8_t* options = buffer + sizeof(dhcp_header_t);
    options[0] = 53;  /* Opção 53: DHCP Message Type */
    options[1] = 1;   /* Tamanho = 1 byte */
    options[2] = 1;   /* Valor = 1 (DHCP DISCOVER) */
    options[3] = 255; /* Opção 255: Fim das opções (End Option) */

    /* 3. Configura o Endereço de Destino IP/Porta em Broadcast total */
    struct sockaddr_in dest_broadcast;
    dest_broadcast.sin_family = AF_INET;
    dest_broadcast.sin_port = htons(67);             /* Porta do servidor DHCP */
    dest_broadcast.sin_addr = htonl(0xFFFFFFFF);     /* 255.255.255.255 */
    memset(dest_broadcast.sin_zero, 0, 8);

    kprintf("[DHCP] Transmitindo DHCP DISCOVER em Broadcast (XID: 0x%x)...\n", DHCP_DISCOVER_XID);

    /* Dispara via driver UDP polimórfico */
    long res = udp_send_datagram(sock, buffer, payload_size, &dest_broadcast);
    kfree(buffer);

    return (res > 0) ? 0 : -3;
}

/**
 * @brief dhcp_send_request - Forja e envia o pacote DHCP REQUEST (Broadcast) para confirmar o IP.
 */
int dhcp_send_request(socket_t* sock, uint32_t requested_ip)
{
    if (!sock) return -1;

    /* Tamanho base + Opção 53 (3b) + Opção 50 (Requested IP - 6b) + Opção End (1b) = 246 bytes */
    uint32_t payload_size = sizeof(dhcp_header_t) + 10;
    uint8_t* buffer = (uint8_t*)kmalloc(payload_size);
    if (!buffer) return -2;
    memset(buffer, 0, payload_size);

    /* 1. Preenche o cabeçalho base idêntico */
    dhcp_header_t* dhcp = (dhcp_header_t*)buffer;
    dhcp->op           = DHCP_BOOTREQUEST;
    dhcp->htype        = 1;
    dhcp->hlen         = 6;
    dhcp->xid          = htonl(DHCP_DISCOVER_XID);
    dhcp->flags        = htons(0x8000);
    memcpy(dhcp->chaddr, g_net_interface_mac, 6);
    dhcp->magic_cookie = htonl(0x63825363);

    /* 2. Escrita sequencial das Opções TLV */
    uint8_t* options = buffer + sizeof(dhcp_header_t);
    
    /* Opção 53: DHCP Message Type = 3 (DHCP REQUEST) */
    options[0] = 53;  options[1] = 1;  options[2] = 3;
    
    /* Opção 50: Requested IP Address (Diz ao servidor qual o IP que queremos fixar) */
    options[3] = 50;  options[4] = 4;
    memcpy(&options[5], &requested_ip, 4);
    
    /* Opção 255: End Option */
    options[9] = 255;

    struct sockaddr_in dest_broadcast;
    dest_broadcast.sin_family = AF_INET;
    dest_broadcast.sin_port   = htons(67);
    dest_broadcast.sin_addr   = htonl(0xFFFFFFFF); /* Sai em Broadcast */
    memset(dest_broadcast.sin_zero, 0, sizeof(dest_broadcast.sin_zero));

    kprintf("[DHCP] Transmitindo DHCP REQUEST para o IP: 0x%x...\n", ntohl(requested_ip));

    long res = udp_send_datagram(sock, buffer, payload_size, &dest_broadcast);
    kfree(buffer);

    return (res > 0) ? 0 : -3;
}

/**
 * @brief dhcp_input - Processa as respostas DHCP (Offers/ACKs) vindas da triagem UDP.
 * 
 * @param data Ponteiro para os dados puros do payload UDP (Porta 68).
 * @param len  Tamanho total dos dados recebidos.
 */
int dhcp_input(const void* data, uint32_t len)
{
    if (!data || len < sizeof(dhcp_header_t)) return -1;

    dhcp_header_t* dhcp = (dhcp_header_t*)data;

    /* Valida se o ID de transação bate com o nosso pedido ativo */
    if (ntohl(dhcp->xid) != DHCP_DISCOVER_XID) return 0;

    if (dhcp->op == DHCP_BOOTREPLY)
    {
        uint32_t offered_ip = dhcp->yiaddr;

        /* Varredura das Opções TLV */
        uint8_t* options = ((uint8_t*)data) + sizeof(dhcp_header_t);
        uint32_t idx = 0;
        uint32_t max_options_len = len - sizeof(dhcp_header_t);

        uint32_t subnet_mask = 0;
        uint32_t gateway_ip = 0;
        uint8_t  dhcp_msg_type = 0;

        while (idx < max_options_len && options[idx] != 255)
        {
            uint8_t opt_type = options[idx];
            if (opt_type == 0) { idx++; continue; }
            
            uint8_t opt_len  = options[idx + 1];
            uint8_t* opt_val = &options[idx + 2];

            if (opt_type == 53) {
                dhcp_msg_type = opt_val[0]; /* 2 = OFFER, 5 = ACK */
            }
            else if (opt_type == 1 && opt_len == 4) {
                memcpy(&subnet_mask, opt_val, 4);
            }
            else if (opt_type == 3 && opt_len == 4) {
                memcpy(&gateway_ip, opt_val, 4);
            }

            idx += 2 + opt_len;
        }

        /* COMPORTAMENTO CONSOANTE O ESTADO DO PROTOCOLO */
        if (dhcp_msg_type == 2) 
        {
            /* FASE 1: Recebeu o OFFER (O Servidor propôs um IP) */
            kprintf("[DHCP Input] DHCP OFFER recebido: 0x%x. A enviar confirmacao...\n", ntohl(offered_ip));
            
            /* 
             * Resgata o socket ativo que iniciou o processo de rede.
             * Como o dhcp_input roda dentro do udp_input, podemos buscar o socket da porta 68
             */
            socket_t* sock = socket_find_by_port(htons(68), SOCK_DGRAM);
            if (sock) {
                dhcp_send_request(sock, offered_ip); /* Avança para o passo REQUEST */
            }
        }
        else if (dhcp_msg_type == 5)
        {
            /* FASE 2: Recebeu o ACK (O Servidor confirmou e fechou o contrato!) */
            g_net_interface_ip      = offered_ip;
            if (subnet_mask) g_net_interface_mask    = subnet_mask;
            if (gateway_ip)  g_net_interface_gateway = gateway_ip;

            kprintf("[DHCP Input] MATCH CONCLUÍDO! Configuração IP atribuida via DHCP:\n");
            kprintf("   -> Meu IP:   %s\n", inet_ntoa(g_net_interface_ip));
            kprintf("   -> Mascara:  %s\n", inet_ntoa(g_net_interface_mask));
            kprintf("   -> Gateway:  %s\n", inet_ntoa(g_net_interface_gateway));
            
        }
    }

    return 0;
}
