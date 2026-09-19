/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: tcp.c
 *    Description: Camada de Transporte TCP (Transmission Control Protocol).
 *                 Gere conexões fiáveis, fluxos contínuos e implementa
 *                 a máquina de estados finitos do Handshake POSIX com suporte
 *                 a múltiplos núcleos SMP.
 * 
 *         Author: Nelson Cole
 *   Created Date: 18/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/socket.h>
#include <kernel/kernel/net/net.h>
#include <kernel/klib.h>
#include <kernel/lib/string.h>

/* Camada inferior do protocolo de internet */
extern int ip_output(uint32_t dest_ip, uint8_t protocol, const void* data, uint32_t len);

/* Definição interna de Flags do Cabeçalho TCP */
#define TCP_FLAG_SYN  0x0002
#define TCP_FLAG_RST  0x0004
#define TCP_FLAG_PSH  0x0008
#define TCP_FLAG_ACK  0x0010

/* Variáveis de controle de fluxo temporárias (Progressão de Handshake/Sequence) */
static uint32_t g_tcp_local_seq  = 1234567;
static uint32_t g_tcp_remote_ack = 0;

/* Estrutura do Pseudo-Cabeçalho IPv4 necessário para o cálculo do Checksum TCP */
typedef struct {
    uint32_t src_ip;
    uint32_t dest_ip;
    uint8_t  reserved;
    uint8_t  protocol;
    uint16_t tcp_len;
} __attribute__((packed)) tcp_pseudo_header_t;

/**
 * @brief Calcula o Checksum avançado do TCP incluindo o Pseudo-Cabeçalho IP (RFC 793).
 */
static uint16_t tcp_calculate_checksum(tcp_pseudo_header_t* pseudo, tcp_header_t* tcp, const void* data, uint32_t data_len)
{
    uint32_t sum = 0;
    
    /* 1. Soma os blocos de 16 bits do Pseudo-Cabeçalho */
    uint16_t* ptr = (uint16_t*)pseudo;
    for (size_t i = 0; i < sizeof(tcp_pseudo_header_t) / 2; i++) {
        sum += ptr[i];
    }

    /* 2. Soma os blocos de 16 bits do Cabeçalho TCP */
    ptr = (uint16_t*)tcp;
    for (size_t i = 0; i < sizeof(tcp_header_t) / 2; i++) {
        sum += ptr[i];
    }

    /* 3. Soma os blocos de 16 bits do Payload de dados (se existirem) */
    ptr = (uint16_t*)data;
    uint32_t length = data_len;
    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }
    if (length > 0) {
        sum += *(uint8_t*)ptr;
    }

    /* Dobra os bits de carry de 32 para 16 bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}


/**
 * @brief tcp_connect_handshake - Constrói e envia o pacote inicial SYN para o servidor.
 */
int tcp_connect_handshake(socket_t* sock, struct sockaddr_in* dest)
{
    if (!sock || !dest) return -1;

    /* O pacote SYN inicial não carrega dados, possui apenas o cabeçalho TCP (20 bytes) */
    uint32_t tcp_packet_size = sizeof(tcp_header_t);

    uint8_t* tcp_buffer = (uint8_t*)kmalloc(tcp_packet_size);
    if (!tcp_buffer) return -2;
    memset(tcp_buffer, 0, tcp_packet_size);

    struct sockaddr_in* local = (struct sockaddr_in*)sock->local_addr;
    uint16_t src_port = local ? local->sin_port : htons(6000); 

    tcp_header_t* tcp = (tcp_header_t*)tcp_buffer;
    tcp->src_port   = src_port;
    tcp->dest_port  = dest->sin_port;
    tcp->seq_num    = htonl(g_tcp_local_seq); 
    tcp->ack_num    = htonl(0);       
    
    /* Configura o Data Offset = 5 (20 bytes) e ativa estritamente a flag SYN (0x0002) */
    tcp->data_offset_flags = htons((5 << 12) | TCP_FLAG_SYN); 
    tcp->window_size       = htons(1024); 
    tcp->checksum          = 0x0000;
    tcp->urgent_ptr        = 0x0000;

    /* Configuração do Pseudo-Cabeçalho IP para o Cálculo Obrigatório do Checksum */
    tcp_pseudo_header_t pseudo;
    /* Lê o IP de origem em Network Byte Order diretamente da global.*/
    pseudo.src_ip   = g_net_interface_ip; 
    pseudo.dest_ip  = dest->sin_addr;
    pseudo.reserved = 0;
    pseudo.protocol = IPPROTO_TCP;       
    pseudo.tcp_len  = htons(tcp_packet_size);

    /* O payload de dados é nulo (NULL, 0) no aperto de mão inicial */
    tcp->checksum = tcp_calculate_checksum(&pseudo, tcp, NULL, 0);

    kprintf("[TCP] Enviando pacote SYN (Seq: %u) para Porta %d.\n", 
            g_tcp_local_seq, ntohs(tcp->dest_port));

    /* POVOAMENTO ATÓMICO DO NOVO CAMPO REMOTE_ADDR NO SOCKET */
    memcpy(sock->remote_addr, dest, sizeof(struct sockaddr_in));
    sock->remote_addr_len = sizeof(struct sockaddr_in);

    sock->state = TCP_STATE_SYN_SENT;

    /* Despacha o segmento montado para a camada de rede IPv4 */
    int res = ip_output(dest->sin_addr, IPPROTO_TCP, tcp_buffer, tcp_packet_size);
    kfree(tcp_buffer);

    if (res < 0) return -3;
    return 0;
}

/**
 * @brief tcp_send_stream - Envia blocos contínuos de bytes através de uma conexão ativa.
 */
long tcp_send_stream(socket_t* sock, const void* buf, unsigned long len)
{
    if (!sock || !buf || len == 0) return -1;

    /* Garante exclusão mútua do estado do socket em ambiente SMP */
    spin_lock(&sock->lock);

    if (sock->state != TCP_STATE_ESTABLISHED) 
    {
        kprintf("[TCP Error] Envio rejeitado: Socket nao esta no estado ESTABLISHED (Estado: %d).\n", sock->state);
        spin_unlock(&sock->lock);
        return -1; 
    }

    /* CAST SEGURO DA MATRIZ DE BYTES DO NOVO CAMPO REMOTE_ADDR */
    struct sockaddr_in* dest = (struct sockaddr_in*)sock->remote_addr;
    if (!dest || sock->remote_addr_len == 0) {
        spin_unlock(&sock->lock);
        return -2;
    }

    uint32_t tcp_packet_size = sizeof(tcp_header_t) + len;

    if ((sizeof(ip_header_t) + tcp_packet_size) > 1500) 
    {
        spin_unlock(&sock->lock);
        return -3;
    }

    uint8_t* tcp_buffer = (uint8_t*)kmalloc(tcp_packet_size);
    if (!tcp_buffer) {
        spin_unlock(&sock->lock);
        return -4;
    }
    memset(tcp_buffer, 0, tcp_packet_size);

    struct sockaddr_in* local = (struct sockaddr_in*)sock->local_addr;
    uint16_t src_port = local ? local->sin_port : htons(6000);

    tcp_header_t* tcp = (tcp_header_t*)tcp_buffer;
    tcp->src_port   = src_port;
    tcp->dest_port  = dest->sin_port;
    tcp->seq_num    = htonl(g_tcp_local_seq);
    tcp->ack_num    = htonl(g_tcp_remote_ack);
    
    tcp->data_offset_flags = htons((5 << 12) | TCP_FLAG_ACK | TCP_FLAG_PSH);
    tcp->window_size       = htons(2048); 
    tcp->checksum          = 0x0000;
    tcp->urgent_ptr        = 0x0000;

    char* tcp_data_space = (char*)(tcp_buffer + sizeof(tcp_header_t));
    memcpy(tcp_data_space, buf, len);

    /* MOLDAGEM DO PSEUDO-CABEÇALHO IP ATÓMICO */
    tcp_pseudo_header_t pseudo;
    //Puxa o IP de origem em Network Byte Order direto da global provisória.
    pseudo.src_ip   = g_net_interface_ip; 
    pseudo.dest_ip  = dest->sin_addr;
    pseudo.reserved = 0;
    pseudo.protocol = IPPROTO_TCP;       
    pseudo.tcp_len  = htons(tcp_packet_size);

    tcp->checksum = tcp_calculate_checksum(&pseudo, tcp, tcp_data_space, len);

    kprintf("[TCP] Despachando Segmento DATA: %d bytes (Seq: %u, Ack: %u)\n", 
            len, g_tcp_local_seq, g_tcp_remote_ack);

    /* Avança o número de sequência local antes de libertar o trinco e transmitir */
    g_tcp_local_seq += len;
    spin_unlock(&sock->lock);

    int res = ip_output(dest->sin_addr, IPPROTO_TCP, tcp_buffer, tcp_packet_size);
    kfree(tcp_buffer);

    if (res < 0) return -5;
    return (long)len; 
}

/**
 * @brief tcp_input - Processa e decodifica segmentos TCP recebidos da camada IP (Bottom-Half/RX).
 *                    Gere o avanço da máquina de estados e alimenta o Ring Buffer do socket.
 * 
 * @param data     Ponteiro para o início do segmento TCP (Cabeçalho + Payload).
 * @param len      Tamanho total do segmento vindo da camada IP.
 * @param src_ip   Endereço IPv4 da máquina remota que enviou o pacote.
 * @return 0 em caso de sucesso absoluto, ou valor negativo em caso de falha/descarte.
 */
int tcp_input(const void* data, uint32_t len, uint32_t src_ip)
{
    (void)src_ip;
    if (!data || len < sizeof(tcp_header_t)) return -1;

    /* 1. Mapeia a estrutura sobre o buffer de entrada e isola as flags */
    tcp_header_t* tcp = (tcp_header_t*)data;
    uint16_t flags = ntohs(tcp->data_offset_flags) & 0x0FFF;

    /* 2. Demultiplexação de Porta: Localiza o socket TCP alvo no Kernel */
    socket_t* sock = socket_find_by_port(tcp->dest_port, SOCK_STREAM);
    if (!sock) return 0; /* Descarte silencioso (Porta fechada / Port Unreachable) */

    /* Garante exclusão mútua do Ring Buffer em ambiente SMP */
    spin_lock(&sock->lock);

    /* ====================================================================
     * MÁQUINA DE ESTADOS FINITOS TCP (Handshake & Data Processing)
     * ==================================================================== */
    
    if (sock->state == TCP_STATE_SYN_SENT) 
    {
        /* CASO A: O Sirius enviou um SYN e aguardava a resposta formal do servidor */
        if ((flags & TCP_FLAG_SYN) && (flags & TCP_FLAG_ACK)) 
        {
            kprintf("[TCP Input] SYN-ACK recebido de volta! A sincronizar contadores...\n");
            
            /* Sincroniza os contadores de sequência locais com base nos dados do hardware remoto */
            g_tcp_remote_ack = ntohl(tcp->seq_num) + 1; /* O nosso próximo ACK confirma o SYN deles */
            g_tcp_local_seq  = ntohl(tcp->ack_num);

            /* Transitamos o estado físico do socket para a conectividade total */
            sock->state = TCP_STATE_ESTABLISHED;
            
            /* 
             * FECHO DO THREE-WAY HANDSHAKE:
             * Envia o pacote final ACK puro para confirmar o SYN-ACK e abrir o canal no servidor.
             * Alocamos um cabeçalho TCP simples de 20 bytes (sem payload).
             */
            uint32_t ack_packet_size = sizeof(tcp_header_t);
            uint8_t* ack_buffer = (uint8_t*)kmalloc(ack_packet_size);
            if (ack_buffer) 
            {
                memset(ack_buffer, 0, ack_packet_size);
                tcp_header_t* reply_tcp = (tcp_header_t*)ack_buffer;
                
                reply_tcp->src_port   = tcp->dest_port;
                reply_tcp->dest_port  = tcp->src_port;
                reply_tcp->seq_num    = htonl(g_tcp_local_seq);
                reply_tcp->ack_num    = htonl(g_tcp_remote_ack);
                reply_tcp->data_offset_flags = htons((5 << 12) | TCP_FLAG_ACK); /* Apenas flag ACK ativa */
                reply_tcp->window_size       = htons(2048);
                
                /* Computa o pseudo-cabeçalho IP para o Checksum do ACK de resposta */
                tcp_pseudo_header_t pseudo;
                pseudo.src_ip   = g_net_interface_ip;
                pseudo.dest_ip  = src_ip;
                pseudo.reserved = 0;
                pseudo.protocol = IPPROTO_TCP;
                pseudo.tcp_len  = htons(ack_packet_size);
                
                reply_tcp->checksum = tcp_calculate_checksum(&pseudo, reply_tcp, NULL, 0);
                
                kprintf("[TCP] A enviar ACK final (Seq: %u, Ack: %u) para estabelecer ligacao.\n", 
                        g_tcp_local_seq, g_tcp_remote_ack);
                
                /* Dispara o pacote final para descer a pilha de rede */
                spin_unlock(&sock->lock); /* Solta temporariamente o lock antes de invocar o IP */
                ip_output(src_ip, IPPROTO_TCP, ack_buffer, ack_packet_size);
                kfree(ack_buffer);
                
                return 0; /* Handshake concluído com sucesso absoluto */
            }
        }
    }
    else if (sock->state == TCP_STATE_ESTABLISHED)
    {
        /* CASO B: Canal de comunicação aberto. Processa a entrada contínua de streams */
        uint8_t h_len = ((ntohs(tcp->data_offset_flags) >> 12) & 0x0F) * 4;
        
        if (h_len < sizeof(tcp_header_t) || h_len > len) {
            spin_unlock(&sock->lock);
            return -2; /* Segmento corrompido */
        }

        uint32_t payload_len = len - h_len;

        /* Verifica se há bytes reais de dados acoplados e se o pacote carrega uma flag ACK legítima */
        if (payload_len > 0 && (flags & TCP_FLAG_ACK))
        {

            /* 1. Cálculo de Espaço Livre no Ring Buffer nativo */
            uint32_t free_space = (sock->rx_tail - sock->rx_head - 1 + SOCKET_BUFFER_SIZE) % SOCKET_BUFFER_SIZE;

            if (free_space < payload_len)
            {
                kprintf("[TCP Input] Buffer saturado! A publicitar Janela Zero (Zero Window) para o transmissor...\n");

                /*
                 * SOLUÇÃO ATIVA: Monta e envia um pacote ACK puro imediatamente.
                 * Neste pacote, forçamos o campo 'window_size' a ser ZERO (ou o 'free_space' real restante).
                 */
                uint32_t ack_size = sizeof(tcp_header_t);
                uint8_t *ack_buf = (uint8_t *)kmalloc(ack_size);
                if (ack_buf)
                {
                    memset(ack_buf, 0, ack_size);
                    tcp_header_t *reply_tcp = (tcp_header_t *)ack_buf;

                    reply_tcp->src_port = tcp->dest_port;
                    reply_tcp->dest_port = tcp->src_port;
                    reply_tcp->seq_num = htonl(g_tcp_local_seq);
                    reply_tcp->ack_num = htonl(ntohl(tcp->seq_num)); /* Não avança o ACK porque dropámos os dados */
                    reply_tcp->data_offset_flags = htons((5 << 12) | TCP_FLAG_ACK);

                    /* Avisa o hardware remoto que a nossa janela de receção esgotou */
                    reply_tcp->window_size = htons(free_space);

                    tcp_pseudo_header_t pseudo;
                    pseudo.src_ip = g_net_interface_ip;
                    pseudo.dest_ip = src_ip;
                    pseudo.reserved = 0;
                    pseudo.protocol = IPPROTO_TCP;
                    pseudo.tcp_len = htons(ack_size);
                    reply_tcp->checksum = tcp_calculate_checksum(&pseudo, reply_tcp, NULL, 0);

                    spin_unlock(&sock->lock);
                    ip_output(src_ip, IPPROTO_TCP, ack_buf, ack_size);
                    kfree(ack_buf);
                    return 0; /* Retorna sem corromper a memória, o host remoto vai retransmitir mais tarde */
                }

                spin_unlock(&sock->lock);
                return -3;
            }

            kprintf("[TCP Input] Recebidos %d bytes de stream de dados. A gravar no rx_buffer...\n", payload_len);

            /* 2. Extrai o ponteiro do payload puro (salta o tamanho exato do cabeçalho dinâmico) */
            const uint8_t *tcp_payload = ((const uint8_t *)data) + h_len;

            /* 3. INJEÇÃO CIRCULAR DE BYTES (Assimétrico ao Ring Buffer nativo) */
            for (uint32_t i = 0; i < payload_len; i++)
            {
                sock->rx_buffer[sock->rx_head] = tcp_payload[i];
                sock->rx_head = (sock->rx_head + 1) % SOCKET_BUFFER_SIZE;
            }

            /* 4. Atualiza o número de confirmação (Ack) local com base nos bytes que consumimos */
            g_tcp_remote_ack = ntohl(tcp->seq_num) + payload_len;

            /*
             * 5. FECHO DO CICLO DE RECEÇÃO (ENVIO DO ACK DE CONFIRMAÇÃO DE DADOS):
             * Aloca e envia um pacote ACK de volta para o transmissor para confirmar
             * o recebimento dos bytes e atualizar a nossa Janela Deslizante ativa.
             */
            uint32_t ack_reply_size = sizeof(tcp_header_t);
            uint8_t *ack_reply_buf = (uint8_t *)kmalloc(ack_reply_size);

            if (ack_reply_buf)
            {
                memset(ack_reply_buf, 0, ack_reply_size);
                tcp_header_t *reply_tcp = (tcp_header_t *)ack_reply_buf;

                /* Inverte as portas lógicas para o retorno */
                reply_tcp->src_port = tcp->dest_port;
                reply_tcp->dest_port = tcp->src_port;

                /* O nosso número de sequência é o atual; o ACK confirma até onde já lemos */
                reply_tcp->seq_num = htonl(g_tcp_local_seq);
                reply_tcp->ack_num = htonl(g_tcp_remote_ack);

                /* Configura Data Offset = 5 (20 bytes) e ativa estritamente a flag ACK (0x0010) */
                reply_tcp->data_offset_flags = htons((5 << 12) | TCP_FLAG_ACK);

                /* CONTROLO DE FLUXO ATIVO: Calcula e anuncia a nova janela livre do rx_buffer */
                uint32_t current_free_space = (sock->rx_tail - sock->rx_head - 1 + SOCKET_BUFFER_SIZE) % SOCKET_BUFFER_SIZE;
                if (current_free_space > 0xFFFF)
                    current_free_space = 0xFFFF;
                reply_tcp->window_size = htons((uint16_t)current_free_space);

                /* Computa o Pseudo-Cabeçalho IP para validação física remota */
                tcp_pseudo_header_t pseudo;
                pseudo.src_ip = g_net_interface_ip; /* Global unificada do net.c */
                pseudo.dest_ip = src_ip;
                pseudo.reserved = 0;
                pseudo.protocol = IPPROTO_TCP;
                pseudo.tcp_len = htons(ack_reply_size);

                reply_tcp->checksum = tcp_calculate_checksum(&pseudo, reply_tcp, NULL, 0);

                kprintf("[TCP] A enviar ACK de confirmacao de dados (Seq: %u, Ack/Próximo: %u, Janela Livre: %u).\n",
                        g_tcp_local_seq, g_tcp_remote_ack, current_free_space);

                /* Desativa temporariamente o lock do socket antes de chamar a camada inferior IP */
                spin_unlock(&sock->lock);
                ip_output(src_ip, IPPROTO_TCP, ack_reply_buf, ack_reply_size);
                kfree(ack_reply_buf);

                return 0; /* Segmento processado e confirmado de forma canónica */
            }
        }
    }

    spin_unlock(&sock->lock);
    return 0; /* Sucesso */
}