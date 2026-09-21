/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: af_inet.c
 *    Description: Driver polimórfico completo para a família de protocolos 
 *                 Internet IPv4 (AF_INET). Centraliza ganchos para as futuras
 *                 camadas da pilha TCP/UDP/IP e injeção em drivers físicos.
 * 
 *         Author: Nelson Cole
 *   Created Date: 17/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 18/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */
#include <kernel/kernel/net/socket.h>
#include <kernel/kernel/net/net.h>
#include <kernel/lib/stddef.h>
#include <kernel/lib/string.h>
#include <kernel/klib.h>


/* Declarações das rotinas externas e nativas da pilha TCP/UDP */
extern int  tcp_connect_handshake(socket_t* sock, struct sockaddr_in* dest);
extern long tcp_send_stream(socket_t* sock, const void* buf, unsigned long len);
extern long udp_send_datagram(socket_t* sock, const void* buf, unsigned long len, struct sockaddr_in* dest);

/**
 * @brief Associa um endereço IP e uma Porta (Port) ao socket de internet.
 */
static int af_inet_bind(socket_t* sock, const void* addr, unsigned long addrlen) 
{
    if (!sock || !addr || addrlen < sizeof(struct sockaddr_in)) return -1;

    struct sockaddr_in* sin = (struct sockaddr_in*)addr;
    if (sin->sin_family != AF_INET) return -2;

    /* Delega a validação de portas duplicadas e o encadeamento para o socket.c */
    int res = socket_bind_address(sock, addr, sizeof(struct sockaddr_in));
    if (res < 0) 
    {
        kprintf("[AF_INET] Erro: Endereco/Porta ja em uso ou falha no bind.\n");
        return res;
    }

    return 0;
}

/**
 * @brief Solicita uma conexão atómica ponto-a-ponto com o IP/Porta remoto.
 */
static int af_inet_connect(socket_t* sock, const void* addr, unsigned long addrlen) 
{
    if (!sock || !addr || addrlen < sizeof(struct sockaddr_in)) return -1;

    struct sockaddr_in* dest_sin = (struct sockaddr_in*)addr;
    if (dest_sin->sin_family != AF_INET) return -2;

    /* Captura atómica do endereço remoto no novo campo estruturado */
    spinlock_acquire(&sock->lock);
    memcpy(sock->remote_addr, addr, sizeof(struct sockaddr_in));
    sock->remote_addr_len = sizeof(struct sockaddr_in);
    spinlock_release(&sock->lock);

    /* Triagem da camada de transporte com base no tipo do socket */
    if (sock->type == SOCK_STREAM) 
    {
        /* TCP: Inicializa o aperto de mão síncrono (SYN) */
        return tcp_connect_handshake(sock, dest_sin);
    } 
    else if (sock->type == SOCK_DGRAM) 
    {
        /* UDP: Sem ligação. Apenas fixa o destino para futuros envios pelo send() */
        spinlock_acquire(&sock->lock);
        sock->state = 1; /* Connected lógico para o Ring 3 */
        spinlock_release(&sock->lock);
        
        kprintf("[AF_INET] Connect UDP concluido.\n");
        return 0;
    }

    return -3;
}

/**
 * @brief Aceita uma nova conexão de rede vinda da fila do servidor passivo.
 */
static socket_t* af_inet_accept(socket_t* sock) 
{
    spinlock_acquire(&sock->lock);
    if (!sock || sock->state != 2) 
    {
        spinlock_release(&sock->lock);
        return NULL;
    }
    spinlock_release(&sock->lock);

    while (sock->listen_queue == NULL) 
    {
        __asm__ __volatile__("pause");
    }

    /* Protege a extração da fila local usando o lock do próprio socket */
    spinlock_acquire(&sock->lock);
    socket_t* client_sock = sock->listen_queue;
    if (client_sock) 
    {
        sock->listen_queue = client_sock->next;
        client_sock->next = NULL;
    }
    spinlock_release(&sock->lock);

    return client_sock; 
}

/**
 * @brief Coloca o socket de internet em modo passivo de escuta.
 */
static int af_inet_listen(socket_t* sock, int backlog) 
{
    (void)backlog;
    if (!sock) return -1;
    if (sock->type != SOCK_STREAM) return -2; /* UDP não suporta modo listen */

    spinlock_acquire(&sock->lock);
    sock->state = 2; 
    spinlock_release(&sock->lock);
    
    return 0;
}

/**
 * @brief Transmite uma mensagem (datagrama ou stream) para a pilha IP/Hardware.
 */
static long af_inet_sendto(socket_t* sock, const void* buf, unsigned long len, int flags, const void* dest_addr, unsigned long addrlen) 
{
    (void)flags;
    if (!sock || !buf || len == 0) return -1;

    if (sock->type == SOCK_STREAM) 
    {
        /* TCP: Fluxo fiável orientado a stream */
        return tcp_send_stream(sock, buf, len);
    } 
    else if (sock->type == SOCK_DGRAM) 
    {
        /* UDP: Comunicação orientada a Datagramas discretos */
        struct sockaddr_in* dest = (struct sockaddr_in*)dest_addr;
        
        /* Caso tenha omitido o endereço no sendto mas realizou um connect() prévio */
        if (!dest && addrlen == 0) 
        {
            spinlock_acquire(&sock->lock);
            if (sock->state == 1 && sock->remote_addr_len >= sizeof(struct sockaddr_in)) 
            {
                dest = (struct sockaddr_in*)sock->remote_addr;
            }
            spinlock_release(&sock->lock);
        }
        
        if (!dest) return -2;
        return udp_send_datagram(sock, buf, len, dest);
    }

    return -3;
}

/**
 * @brief Captura e consome mensagens de rede unicamente do seu rx_buffer de entrada.
 */
static long af_inet_recvfrom(socket_t* sock, void* buf, unsigned long len, int flags, void* src_addr, unsigned long* addrlen) 
{
    (void)flags;
    if (!sock || !buf || len == 0 || sock->rx_buffer == NULL) return -1;

    uint8_t* dest = (uint8_t*)buf;
    uint32_t bytes_read = 0;

    /* Extração exclusiva e protegida do buffer circular do socket */
    spinlock_acquire(&sock->lock);

    if (sock->type == SOCK_DGRAM)
    {
        /* 
         * BLOQUEIO SEGURO UDP (Aguardar Mensagem):
         * Se o buffer estiver completamente vazio, a thread cede o CPU de forma passiva 
         * até que o udp_input receba um datagrama e avance o rx_head.
         */
        while (sock->rx_head == sock->rx_tail) 
        {
            spinlock_release(&sock->lock);
            
            /* Coloca o core local em repouso passivo (ou chame scheduler_yield()) */
            __asm__ __volatile__("hlt"); 
            
            spinlock_acquire(&sock->lock);
        }

        /* Passo A: Extrai o tamanho real do datagrama (4 bytes) */
        uint32_t user_data_len = 0;
        uint8_t* len_ptr = (uint8_t*)&user_data_len;
        for (uint32_t i = 0; i < sizeof(uint32_t); i++) {
            len_ptr[i] = sock->rx_buffer[sock->rx_tail];
            sock->rx_tail = (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
        }

        /* Passo B: Extrai a estrutura sockaddr_in do remetente (16 bytes) */
        struct sockaddr_in source_addr;
        uint8_t* addr_ptr = (uint8_t*)&source_addr;
        for (uint32_t i = 0; i < sizeof(struct sockaddr_in); i++) {
            addr_ptr[i] = sock->rx_buffer[sock->rx_tail];
            sock->rx_tail = (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
        }

        /* Preenche os parâmetros exigidos pelas syscalls do Ring 3 */
        if (src_addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
            memcpy(src_addr, &source_addr, sizeof(struct sockaddr_in));
            *addrlen = sizeof(struct sockaddr_in);
        }

        /* Passo C: Transfere os dados puros para o buffer do utilizador */
        uint32_t limit = (user_data_len < len) ? user_data_len : len;
        while (bytes_read < limit) 
        {
            dest[bytes_read] = sock->rx_buffer[sock->rx_tail];
            sock->rx_tail = (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
            bytes_read++;
        }

        /* Se o buffer do app for menor que o pacote, descarta os bytes excedentes para manter o alinhamento */
        if (bytes_read < user_data_len) {
            sock->rx_tail = (sock->rx_tail + (user_data_len - bytes_read)) % SOCKET_BUFFER_SIZE;
        }
    }
    else if (sock->type == SOCK_STREAM)
    {
        /* BLOQUEIO SEGURO TCP */
        while (sock->rx_head == sock->rx_tail && sock->state == TCP_STATE_ESTABLISHED) 
        {
            spinlock_release(&sock->lock);
            __asm__ __volatile__("hlt"); 
            spinlock_acquire(&sock->lock);
        }

        if (sock->rx_head == sock->rx_tail && sock->state != TCP_STATE_ESTABLISHED)
        {
            spinlock_release(&sock->lock);
            return 0; /* EOF */
        }

        /* Filtro TCP: Consumo linear de fluxo de bytes puros */
        while (sock->rx_head != sock->rx_tail && bytes_read < len) 
        {
            dest[bytes_read] = sock->rx_buffer[sock->rx_tail];
            sock->rx_tail = (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
            bytes_read++;
        }

        if (bytes_read > 0 && src_addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
            memcpy(src_addr, sock->remote_addr, sizeof(struct sockaddr_in));
            *addrlen = sizeof(struct sockaddr_in);
        }
    }

    spinlock_release(&sock->lock);
    return (long)bytes_read;
}

/* ============================================================================
 * EXPORTAÇÃO COMPLETA DA TABELA POLIMÓRFICA DO PROTOCOLO INTERNET (IPv4)
 * ============================================================================
 */
protocol_operations_t g_af_inet_ops = {
    .bind     = af_inet_bind,
    .connect  = af_inet_connect,
    .sendto   = af_inet_sendto,
    .recvfrom = af_inet_recvfrom,
    .listen   = af_inet_listen,
    .accept   = af_inet_accept
};