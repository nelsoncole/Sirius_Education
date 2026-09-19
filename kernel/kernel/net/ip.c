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
 *  Modified Date: 18/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/net.h>
#include <kernel/klib.h>

/* Contador estático e global para identificação exclusiva de fragmentos de pacotes */
static uint16_t g_ip_packet_id = 0;

/**
 * @brief Calcula o Internet Checksum clássico de 16 bits (RFC 1071).
 *        Utilizado para validar a integridade estrutural do cabeçalho IP.
 */
static uint16_t ip_calculate_checksum(void* data, size_t length) 
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
    uint32_t total_packet_size = sizeof(ip_header_t) + len;

    /* 
     * VALIDAÇÃO DE LIMITE CRÍTICO DE REDE: 1500 bytes é a MTU clássica da Internet.
     * Pacotes maiores exigiriam fragmentação física, não suportada nesta fase do Kernel.
     */
    if (total_packet_size > 1500) 
    {
        kprintf("[IPv4 Error] Tamanho total (%d bytes) excede o limite da MTU de 1500.\n", total_packet_size);
        return -2;
    }

    /* 
     * ALOCAÇÃO DINÂMICA SEGURA: Aloca o bloco completo no Heap de Ring 0.
     * Isto blinda a pilha da Thread atual contra estouros de memória (Stack Overflow).
     */
    uint8_t* tx_buffer = (uint8_t*)kmalloc(total_packet_size);
    if (!tx_buffer) 
    {
        kprintf("[IPv4 Error] Falha critica: Memoria insuficiente para alocar o pacote IP.\n");
        return -3;
    }

    /* 1. Mapeia a estrutura do cabeçalho IP de 20 bytes no início exato do bloco de RAM */
    ip_header_t* ip = (ip_header_t*)tx_buffer;
    
    ip->version_ihl     = (4 << 4) | 5;     /* Versão 4 do IP, IHL = 5 (5 palavras * 4 = 20 bytes) */
    ip->tos             = 0x00;             /* Tipo de Serviço padrão (Routine/Normal) */
    ip->total_len       = htons(total_packet_size); /* Tamanho final completo convertido para Big-Endian */
    uint16_t current_id = g_ip_packet_id++;
    ip->id              = htons(current_id);/* Atribui o ID incremental e atualiza o contador */
    ip->flags_fragment  = htons(0x4000);    /* Ativa rigidamente a flag DF (Don't Fragment) */
    ip->ttl             = 64;               /* Tempo de vida do pacote (Time to Live) padrão Unix */
    ip->protocol        = protocol;         /* Liga polimorficamente com TCP (6) ou UDP (17) */
    ip->checksum        = 0x0000;           /* Zera o campo preventivamente para rodar o cálculo */
    
    /* 
     * ACESSOS GLOBAIS ABSTRATOS:
     * Puxa o IP de origem em tempo real a partir da global provisória do seu net.c
     */
    ip->src_ip          = g_net_interface_ip; 
    ip->dest_ip         = dest_ip;

    /* 2. Calcula a soma de verificação real sobre os 20 bytes do cabeçalho preenchido */
    ip->checksum = ip_calculate_checksum(ip, sizeof(ip_header_t));

    /* 3. Acopla os bytes da Camada de Transporte imediatamente após os 20 bytes do IP */
    char* ip_payload_space = (char*)(tx_buffer + sizeof(ip_header_t));
    memcpy(ip_payload_space, data, len);

    kprintf("[IPv4] Segmento encapsulado no Heap (ID: %d, Protocolo: %d, Total: %d bytes).\n", 
            ntohs(ip->id), ip->protocol, total_packet_size);

    /* 
     * INTERFACE COM O CONECTOR DE HARDWARE FÍSICO:
     * Aqui, a sua camada de rede IPv4 invocará a rotina de transmissão da sua placa ativa:
     */

    int res = net_driver_transmit(tx_buffer, total_packet_size);

    /* 4. Limpa o Heap do Kernel após o despacho */
    kfree(tx_buffer);

    return res; /* Retorno com sucesso absoluto */
}


/* Declarações das funções de entrada das camadas superiores de transporte */
extern int tcp_input(const void* data, uint32_t len, uint32_t src_ip);
extern int udp_input(const void* data, uint32_t len, uint32_t src_ip);

/**
 * @brief ip_input - Processa um pacote IPv4 bruto recebido pelo driver da placa de rede.
 * 
 * @param packet_data Ponteiro para o início do pacote recebido (geralmente vindo do buffer DMA).
 * @param packet_len  Tamanho total do pacote recebido em bytes.
 * @return 0 em caso de sucesso, ou valor negativo em caso de erro/descarte.
 */
int ip_input(const void* packet_data, uint32_t packet_len)
{
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
        // kprintf("[IPv4 Input] Pacote descartado: Versao %d nao suportada.\n", version);
        return -2;
    }

    /* 3. Calcula o tamanho real do cabeçalho IP (IHL * 4) */
    uint8_t ihl = (ip->version_ihl & 0x0F) * 4;
    if (ihl < sizeof(ip_header_t) || ihl > packet_len) 
    {
        return -3; /* Cabeçalho corrompido */
    }

    /* 4. Validação do Internet Checksum de Entrada */
    /* O algoritmo RFC 1071 diz que a soma sobre um cabeçalho válido com o checksum incluso deve dar 0 */
    if (ip_calculate_checksum(ip, ihl) != 0) 
    {
        kprintf("[IPv4 Input] Erro: Checksum invalido. Pacote descartado.\n");
        return -4;
    }

    /* 5. Filtro de Endereço de Destino (IP Match) */
    /* 
     * O pacote deve ser especificamente para o IP ativo da interface do Sirius OS 
     * (lido dinamicamente da global g_net_interface_ip) ou para o endereço de Broadcast.
     */
    uint32_t broadcast_ip = htonl(0xFFFFFFFF);

    if (ip->dest_ip != g_net_interface_ip && ip->dest_ip != broadcast_ip) 
    {
        /* 
         * O pacote não é para nós (outra máquina na rede local). 
         * Descarta silenciosamente para não inundar o Kernel com processamento inútil.
         */
        return 0; 
    }

    /* 6. Extrai o ponteiro e o tamanho real do Payload de Transporte (dados após o cabeçalho IP) */
    const uint8_t* ip_payload = ((const uint8_t*)packet_data) + ihl;
    uint32_t ip_payload_len = ntohs(ip->total_len) - ihl;

    /* 7. DESPACHO POLIMÓRFICO PARA A CAMADA DE TRANSPORTE CORRETA */
    switch (ip->protocol) 
    {
        case IPPROTO_UDP: /* Protocolo 17 */
            // kprintf("[IPv4 Input] Encaminhando payload para a camada UDP (%d bytes).\n", ip_payload_len);
            return udp_input(ip_payload, ip_payload_len, ip->src_ip);

        case IPPROTO_TCP: /* Protocolo 6 */
            // kprintf("[IPv4 Input] Encaminhando payload para a camada TCP (%d bytes).\n", ip_payload_len);
            return tcp_input(ip_payload, ip_payload_len, ip->src_ip);

        default:
            /* Protocolo desconhecido ou não suportado (ex: ICMP, IGMP). Descarta. */
            // kprintf("[IPv4 Input] Protocolo %d nao suportado. Descartado.\n", ip->protocol);
            return -5;
    }
}