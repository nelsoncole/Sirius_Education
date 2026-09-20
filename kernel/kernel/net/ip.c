/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ip.c
 *    Description: Camada de Rede IPv4. Responsável por encapsular payloads
 *                 das camadas superiores, estruturar cabeçalhos e calcular
 *                 o Internet Checksum obrigatório de Ring 0.
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

#include <kernel/kernel/net/net.h>
#include <kernel/klib.h>

/* Contador estático e global para identificação exclusiva de fragmentos de pacotes */
static uint16_t g_ip_packet_id = 0;

/* Protótipos de suporte da Camada de Transporte */
extern int udp_input(const void* data, uint32_t len, uint32_t src_ip);
extern int tcp_input(const void* data, uint32_t len, uint32_t src_ip);
extern int icmp_input(const void* data, uint32_t len, uint32_t src_ip, ip_header_t* ip_hdr);
extern int arp_cache_lookup(uint32_t ip_addr, uint8_t *mac_addr);
extern int arp_request(uint32_t target_ip);


/**
 * @brief Calcula o Internet Checksum clássico de 16 bits (RFC 1071).
 *        Utilizado para validar a integridade estrutural do cabeçalho IP.
 */
uint16_t ip_calculate_checksum(void* data, size_t length) 
{
    uint32_t sum = 0;
    uint16_t* ptr = (uint16_t*)data;

    /* Soma blocos consecutivos de 16 bits */
    while (length > 1) 
    {
        sum += *ptr++;
        length -= 2;
    }

    /* Caso o comprimento seja ímpar, soma o byte residual */
    if (length > 0) 
    {
        sum += *(uint8_t*)ptr;
    }

    /* Dobra os bits de estouro (carry) de 32 bits de volta para 16 bits */
    while (sum >> 16) 
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    /* Retorna o complemento de um (inversão de bits) */
    return (uint16_t)(~sum);
}

/**
 * @brief ip_output - Encapsula e transmite um pacote IPv4 na rede de forma dinâmica.
 *                    Aloca memória no Heap para proteger a stack de Ring 0 contra transbordamento.
 * 
 * @param dest_ip   Endereço IPv4 de destino (Network Byte Order / Big-Endian).
 * @param protocol  Código da camada de transporte (IPPROTO_TCP ou IPPROTO_UDP).
 * @param data      Ponteiro para os dados contíguos vindos de cima (TCP/UDP).
 * @param len       Tamanho do payload de transporte em bytes.
 * @return 0 em caso de sucesso absoluto, ou valor negativo em caso de descarte/falha.
 */
int ip_output(uint32_t dest_ip, uint8_t protocol, const void* data, uint32_t len) 
{
    /* Validação preventiva básica de integridade dos argumentos */
    if (!data && len > 0) return -1;

    /* O tamanho total do pacote engloba os 20 bytes do IP + os bytes da camada de transporte */
    uint32_t ip_packet_size = sizeof(ip_header_t) + len;

     /* O tamanho total real que vai para o cabo inclui os 14 bytes da Ethernet */
    uint32_t total_frame_size = sizeof(ethernet_header_t) + ip_packet_size;

    /* Validação de MTU clássica da Internet (1500 bytes de payload máximo na Ethernet) */
    if (ip_packet_size > 1500) 
    {
        kprintf("[IPv4 Error] Tamanho IP (%d bytes) excede a MTU de 1500.\n", ip_packet_size);
        return -2;
    }

    /* Reserva espaço para Ethernet + IP + Payload */
    uint8_t* tx_buffer = (uint8_t*)kmalloc(total_frame_size);
    if (!tx_buffer) 
    {
        kprintf("[IPv4 Error] Falha critica: Memoria insuficiente para alocar o pacote IP.\n");
        return -3;
    }

    /* 1. Configuração da camada 2 (Ethernet)  */
    ethernet_header_t* eth = (ethernet_header_t*)tx_buffer;
    /* EtherType obrigatório para IPv4: 0x0800 (Convertido para Big-Endian) */
    eth->ethertype = htons(0x0800);
    /* Nossa endereço MAC*/
    net_get_interface_mac(0, eth->src_mac);
    
    /* ALGORITMO DE RESOLUÇÃO DE ENDEREÇO MAC (ARP Gateway/Local) */
    uint32_t target_ip = dest_ip;
    uint32_t broadcast_ip = 0xFFFFFFFF;

    if (dest_ip == broadcast_ip || dest_ip == 0x00000000) 
    {
        /* CASO A: É um pacote de Broadcast legítimo (ex: DHCP Discover/Request) */
        memset(eth->dest_mac, 0xFF, 6);
    } 
    else 
    {
        /* CASO B: É Unicast. Procura o MAC do IP de destino na Cache local */
        uint8_t cached_mac[6];
        if (arp_cache_lookup(target_ip, cached_mac) == 0)
        {
            /* Sucesso! O MAC foi encontrado na Cache. Copia-o para o frame */
            memcpy(eth->dest_mac, cached_mac, 6);
        } 
        else 
        {
            /* 
             * FALHA DE MAPEAMENTO: Não sabemos quem tem este IP.
             * 1. Liberta o buffer atual para evitar fugas de memória (Memory Leak) no Heap.
             * 2. Forja e envia um pacote ARP Request em Broadcast para descobrir o MAC.
             */
            kfree(tx_buffer);
            
            kprintf("[IPv4] MAC nao encontrado para o IP %s. Disparando ARP Request...\n", inet_ntoa(target_ip));
            arp_request(target_ip);
            
            /* 
             * Retorna um código específico indicando que o pacote foi adiado/descartado 
             * enquanto a Camada 2.5 (ARP) resolve o endereço físico do destinatário.
             */
            return -11; 
        }
    }

    /* 2. Configuração da camada 3 (IPv4) */
    /* O IP começa exatamente deslocado após os 14 bytes do cabeçalho Ethernet */
    ip_header_t* ip = (ip_header_t*)(tx_buffer + sizeof(ethernet_header_t));

    ip->version_ihl     = (4 << 4) | 5;     
    ip->tos             = 0x00;             
    ip->total_len       = htons(ip_packet_size); /* Tamanho apenas do pacote IP */
    uint16_t current_id = g_ip_packet_id++;
    ip->id              = htons(current_id);
    ip->flags_fragment  = htons(0x4000);    /* Don't Fragment */
    ip->ttl             = 64;               
    ip->protocol        = protocol;         
    ip->checksum        = 0x0000;
    uint32_t ip_out = 0;
    net_get_interface_ip(0, &ip_out);
    ip->src_ip          = ip_out; 
    ip->dest_ip         = dest_ip;
    
    
    /* Calcula o checksum baseado estritamente nos 20 bytes do cabeçalho IP */
    ip->checksum = ip_calculate_checksum(ip, sizeof(ip_header_t));

    /* 3. Complemento dos dados (Payload) */
    /* Avança o ponteiro saltando a Ethernet e o IP para colar os dados da aplicação */
    uint8_t* ip_payload_space = tx_buffer + sizeof(ethernet_header_t) + sizeof(ip_header_t);
    memcpy(ip_payload_space, data, len);

    /*kprintf("[IPv4] Frame Ethernet montado com Sucesso. (IP ID: %d, Total Frame: %d bytes).\n", 
            ntohs(ip->id), total_frame_size);*/

    /* 
     * INTERFACE COM O HARDWARE FÍSICO:
     * Enviamos o Frame completo (incluindo o cabeçalho Ethernet) para o Driver da e1000
     */
    int res = net_driver_transmit(tx_buffer, total_frame_size);

    /* 4. Limpa o Heap do Kernel */
    kfree(tx_buffer);

    return res; /* Retorno com sucesso absoluto */
}

/**
 * @brief ip_input - Processa um pacote IPv4 bruto recebido pelo driver da placa de rede.
 * 
 * @param packet_data Ponteiro para o início do pacote recebido (geralmente vindo do buffer DMA).
 * @param packet_len  Tamanho total do pacote recebido em bytes.
 * @return 0 em caso de sucesso, ou valor negativo em caso de erro/descarte.
 */
int ip_input(const void* packet_data, uint32_t packet_len)
{
    /* Validação preventiva básica de integridade dos argumentos */
    if (!packet_data || packet_len < sizeof(ip_header_t)) 
    {
        return -1; /* Pacote inválido ou curto demais */
    }

    /* 1. Mapeia a estrutura sobre o buffer recebido */
    ip_header_t* ip = (ip_header_t*)packet_data;

    /* 2. Validação da Versão do Protocolo (Deve ser IPv4) */
    uint8_t version = (ip->version_ihl >> 4) & 0x0F;
    if (version != 4) 
    {
        return -2; /* Versão não suportada */
    }

    /* 3. Calcula o tamanho real do cabeçalho IP (IHL * 4 palavras de 32 bits) */
    uint8_t ihl = (ip->version_ihl & 0x0F) * 4;
    if (ihl < sizeof(ip_header_t) || ihl > packet_len) 
    {
        return -3; /* Cabeçalho corrompido ou IHL inconsistente */
    }

    /* 4. Validação do Internet Checksum de Entrada (RFC 1071) */
    if (ip_calculate_checksum(ip, ihl) != 0) 
    {
        kprintf("[IPv4 Input] Erro: Checksum corrompido. Pacote descartado.\n");
        return -4;
    }

    /* 5. Extração e Sanidade dos Tamanhos do Payload */
    uint16_t total_len = ntohs(ip->total_len);
    if (total_len > packet_len || total_len < ihl)
    {
        return -5; /* Tamanho total declarado no IP é inconsistente com o recebido pelo hardware */
    }

    uint32_t ip_payload_len = total_len - ihl;

    /* 6. FILTRO AVANÇADO DE ENDEREÇO DE DESTINO (IP MATCH) */
    uint32_t broadcast_ip = 0xFFFFFFFF; // 255.255.255.255 em Big/Little Endian é idêntico


    /*
     * CORREÇÃO CRÍTICA PARA DHCP E UNICAST: 
     * O pacote é para nós se:
     *   1. O destino for o nosso IP atual.
     *   2. O destino for Broadcast Geral (255.255.255.255).
     *   3. O nosso IP ainda for 0.0.0.0 (estamos em fase de obtenção de IP via DHCP).
     */
    uint32_t my_ip = 0x10101010;
    net_get_interface_ip(0, &my_ip);
    if (ip->dest_ip != my_ip && 
        ip->dest_ip != broadcast_ip && 
        my_ip != 0x00000000) 
    {
        /* O pacote pertence a outro nó da rede local. Descarta silenciosamente */
        kprintf("Pacote descartado ip->dest_ip %s\n", inet_ntoa(ip->dest_ip));
        return 0; 
    }

    /* Extrai o ponteiro exato de início do payload da Camada 4 */
    const uint8_t* ip_payload = ((const uint8_t*)packet_data) + ihl;

    /* 7. DESPACHO POLIMÓRFICO PARA A CAMADA DE TRANSPORTE CORRETA */
    switch (ip->protocol) 
    {
        case IPPROTO_UDP:
            return udp_input(ip_payload, ip_payload_len, ip->src_ip);

        case IPPROTO_TCP:
            return tcp_input(ip_payload, ip_payload_len, ip->src_ip);

        case IPPROTO_ICMP:
            /* Adicionado suporte a ICMP (Ping) para facilitar os testes do seu OS */
            return icmp_input(ip_payload, ip_payload_len, ip->src_ip, ip);

        default:
            /* Protocolo não suportado na fase atual do Sirius OS */
            return -6;
    }
}
