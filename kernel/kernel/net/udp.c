/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: udp.c
 *    Description: Camada de Transporte UDP (User Datagram Protocol).
 *                 Modifica buffers circulares de sockets (socket_t) e integra
 *                 o fluxo atómico de descida e subida IPv4.
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

#include <kernel/kernel/net/socket.h>
#include <kernel/kernel/net/net.h>
#include <kernel/klib.h>
#include <kernel/lib/string.h>

/* Camada inferior do ecossistema de rede */
extern int ip_output(uint32_t dest_ip, uint8_t protocol, const void* data, uint32_t len);
extern int dhcp_input(const void* data, uint32_t len);
/**
 * @brief udp_send_datagram - Constrói o cabeçalho UDP e encapsula o payload no Heap.
 */
long udp_send_datagram(socket_t* sock, const void* buf, unsigned long len, struct sockaddr_in* dest) 
{
    if (!sock || !buf || !dest || len == 0) return -1;

    uint32_t udp_payload_size = sizeof(udp_header_t) + len;

    if ((sizeof(ip_header_t) + udp_payload_size) > 1500) 
    {
        kprintf("[UDP Error] Datagrama total excede o limite de tamanho da MTU.\n");
        return -2;
    }

    uint8_t* udp_buffer = (uint8_t*)kmalloc(udp_payload_size);
    if (!udp_buffer) return -3;

    struct sockaddr_in* local = (struct sockaddr_in*)sock->local_addr;
    uint16_t src_port = local ? local->sin_port : htons(5000);

    udp_header_t* udp = (udp_header_t*)udp_buffer;
    udp->src_port  = src_port;
    udp->dest_port = dest->sin_port;
    udp->length    = htons(udp_payload_size);
    udp->checksum  = 0x0000; 

    /* Copia linear de dados baseada no Heap dinâmico */
    char* udp_data_space = (char*)(udp_buffer + sizeof(udp_header_t));
    memcpy(udp_data_space, buf, len);

    kprintf("[UDP] Enviando datagrama: Porta %d -> %d (%d bytes)\n", 
            ntohs(udp->src_port), ntohs(udp->dest_port), len);

    int res = ip_output(dest->sin_addr, IPPROTO_UDP, udp_buffer, udp_payload_size);
    
    kfree(udp_buffer);

    if (res < 0) return -4;
    return (long)len;
}

/**
 * @brief udp_input - Descasca o pacote e injeta os bytes e metadados no rx_buffer do socket.
 */
int udp_input(const void* data, uint32_t len, uint32_t src_ip)
{
    if (!data || len < sizeof(udp_header_t)) return -1;

    udp_header_t* udp = (udp_header_t*)data;
    uint32_t udp_len_field = ntohs(udp->length);
    
    if (udp_len_field > len || udp_len_field < sizeof(udp_header_t)) return -2;

    uint32_t user_data_len = udp_len_field - sizeof(udp_header_t);
    const uint8_t* udp_payload = ((const uint8_t*)data) + sizeof(udp_header_t);

    /* 1. Demultiplexação de Porta: Localiza o socket alvo */
    socket_t* target_sock = socket_find_by_port(udp->dest_port, SOCK_DGRAM);
    if (!target_sock || !target_sock->rx_buffer) 
    {
        return 0; /* Descarte silencioso (Port Unreachable) */
    }

    /* DESPACHO POR PORTA: Filtro direcionado ao DHCP */
    if (udp->dest_port == htons(68)) 
    {
        /* O pacote veio do servidor DHCP (porta 67) para o nosso cliente (porta 68) */
        return dhcp_input(udp_payload, user_data_len);
    }

    /* 2. PROTEÇÃO SMP ATÓMICA: Garante exclusão mútua ao alterar o Ring Buffer do socket */
    spin_lock(&target_sock->lock);

    /* 
     * 3. VERIFICAÇÃO DE ESPAÇO LIVRE (Overflow Protection)
     * Para o UDP manter as fronteiras, precisamos de armazenar o tamanho (4 bytes),
     * a estrutura da origem sockaddr_in (16 bytes) e os bytes reais do payload.
     */
    uint32_t metadado_size = sizeof(uint32_t) + sizeof(struct sockaddr_in);
    uint32_t total_required = metadado_size + user_data_len;

    /* Calcula espaço livre no Ring Buffer baseado no SOCKET_BUFFER_SIZE (16384) */
    uint32_t free_space = (target_sock->rx_tail - target_sock->rx_head - 1 + SOCKET_BUFFER_SIZE) % SOCKET_BUFFER_SIZE;

    if (free_space < total_required) 
    {
        kprintf("[UDP Input] Erro: Espaco insuficiente no rx_buffer do Socket. Pacote dropado.\n");
        spin_unlock(&target_sock->lock);
        return -3; /* Buffer saturado, descarta o pacote */
    }

    /* 4. PREPARAÇÃO DO PACOTE DE METADADOS VIRTUAL */
    struct sockaddr_in source_addr;
    source_addr.sin_family = AF_INET;
    source_addr.sin_port   = udp->src_port;
    source_addr.sin_addr   = src_ip;
    memset(source_addr.sin_zero, 0, sizeof(source_addr.sin_zero));

    /* 
     * 5. INJEÇÃO LINEAR / CIRCULAR NO RX_BUFFER
     * Copia sequencialmente avançando o rx_head:
     * Passo A: Tamanho do payload (4 bytes)
     * Passo B: Endereço de origem (16 bytes)
     * Passo C: Bytes puros de dados
     */
    uint8_t* metadata_ptr = (uint8_t*)&user_data_len;
    for (uint32_t i = 0; i < sizeof(uint32_t); i++) {
        target_sock->rx_buffer[target_sock->rx_head] = metadata_ptr[i];
        target_sock->rx_head = (target_sock->rx_head + 1) % SOCKET_BUFFER_SIZE;
    }

    uint8_t* addr_ptr = (uint8_t*)&source_addr;
    for (uint32_t i = 0; i < sizeof(struct sockaddr_in); i++) {
        target_sock->rx_buffer[target_sock->rx_head] = addr_ptr[i];
        target_sock->rx_head = (target_sock->rx_head + 1) % SOCKET_BUFFER_SIZE;
    }

    const uint8_t* raw_user_payload = ((const uint8_t*)data) + sizeof(udp_header_t);
    for (uint32_t i = 0; i < user_data_len; i++) {
        target_sock->rx_buffer[target_sock->rx_head] = raw_user_payload[i];
        target_sock->rx_head = (target_sock->rx_head + 1) % SOCKET_BUFFER_SIZE;
    }

    kprintf("[UDP Input] %d bytes injetados com sucesso no Ring Buffer (Porta: %d).\n", 
            user_data_len, ntohs(udp->dest_port));

    spin_unlock(&target_sock->lock);
    return 0;
}