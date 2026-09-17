/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: af_local.c
 *    Description: Driver polimórfico completo para a família de protocolos 
 *                 de comunicação interna de máquina AF_LOCAL (Domain Sockets).
 *                 Adaptado a 100% para a arquitetura Full-Duplex de duplo buffer.
 * 
 *         Author: Nelson Cole
 *   Created Date: 17/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 17/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/socket.h>
#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/core/spinlock.h>
#include <kernel/klib.h>
#include <kernel/lib/string.h>

// Lista encadeada global de sockets locais registados e o seu lock de barramento
extern socket_t* g_bound_sockets_head;
extern spinlock_t g_socket_list_lock;

/**
 * @brief Associa uma identidade nominal (endereço/caminho) ao socket local.
 */
static int af_local_bind(socket_t* sock, const void* addr, unsigned long addrlen) 
{
    if (!sock || !addr || addrlen == 0 || addrlen > 256) return -1;

    spinlock_acquire(&g_socket_list_lock);

    socket_t* check = g_bound_sockets_head;
    while (check != NULL) 
    {
        if (check->local_addr_len == addrlen && 
            memcmp(check->local_addr, addr, addrlen) == 0) 
        {
            spinlock_release(&g_socket_list_lock);
            kprintf("[AF_LOCAL] Erro: Endereco já em uso.\n");
            return -2; // EADDRINUSE
        }
        check = check->next;
    }

    memcpy(sock->local_addr, addr, addrlen);
    sock->local_addr_len = addrlen;

    sock->next = g_bound_sockets_head;
    g_bound_sockets_head = sock;

    spinlock_release(&g_socket_list_lock);
    return 0;
}

/**
 * @brief Solicita uma conexão atómica ponto-a-ponto com um servidor local.
 */
static int af_local_connect(socket_t* sock, const void* addr, unsigned long addrlen) 
{
    if (!sock || !addr || addrlen == 0) return -1;

    spinlock_acquire(&g_socket_list_lock);

    socket_t* server_sock = g_bound_sockets_head;
    while (server_sock != NULL) 
    {
        if (server_sock->local_addr_len == addrlen && 
            memcmp(server_sock->local_addr, addr, addrlen) == 0) 
        {
            break; 
        }
        server_sock = server_sock->next;
    }

    if (!server_sock || server_sock->state != 2) 
    {
        spinlock_release(&g_socket_list_lock);
        return -1; // Connection refused
    }

    sock->next = server_sock->listen_queue;
    server_sock->listen_queue = sock;

    spinlock_release(&g_socket_list_lock);

    while (sock->state != 1) 
    {
        __asm__ __volatile__("pause");
    }

    return 0;
}

/**
 * @brief Aceita uma conexão pendente da fila do servidor local.
 */
static socket_t* af_local_accept(socket_t* sock) 
{
    if (!sock || sock->state != 2) return NULL;

    while (sock->listen_queue == NULL) 
    {
        __asm__ __volatile__("pause");
    }

    socket_t* client_sock = sock->listen_queue;
    sock->listen_queue = client_sock->next;

    sock->peer = client_sock;
    client_sock->peer = sock;

    sock->state = 1;
    client_sock->state = 1;

    return client_sock;
}

/**
 * @brief Coloca o socket local em modo passivo de escuta.
 */
static int af_local_listen(socket_t* sock, int backlog) 
{
    (void)backlog;
    if (!sock) return -1;
    
    sock->state = 2; 
    return 0;
}

/**
 * @brief Transmite uma mensagem (datagrama) local para o RX do destino.
 */
static long af_local_sendto(socket_t* sock, const void* buf, unsigned long len, int flags, const void* dest_addr, unsigned long addrlen) 
{
    (void)flags;
    if (!sock || !buf || len == 0 || sock->tx_buffer == NULL) return -1;

    socket_t* dest_sock = NULL;

    if (sock->state == 1 && sock->peer) 
    {
        dest_sock = sock->peer;
    }
    else 
    {
        if (!dest_addr || addrlen == 0) return -1;

        spinlock_acquire(&g_socket_list_lock);
        socket_t* curr = g_bound_sockets_head;
        while (curr != NULL) 
        {
            if (curr->local_addr_len == addrlen && 
                memcmp(curr->local_addr, dest_addr, addrlen) == 0) 
            {
                dest_sock = curr;
                break;
            }
            curr = curr->next;
        }
        spinlock_release(&g_socket_list_lock);

        if (!dest_sock) return -1;
    }

    /* 
     * NOVA LÓGICA SYMMETRIC FULL-DUPLEX:
     * Primeiro alimentamos localmente o tx_buffer do transmissor de forma atómica.
     */
    uint8_t* src = (uint8_t*)buf;
    uint32_t bytes_buffered = 0;

    spinlock_acquire(&sock->lock);
    for (uint32_t i = 0; i < len; i++) 
    {
        uint32_t next_tx_tail = (sock->tx_tail + 1) % SOCKET_BUFFER_SIZE;
        if (next_tx_tail == sock->tx_head) break; // Buffer TX cheio

        sock->tx_buffer[sock->tx_tail] = src[i];
        sock->tx_tail = next_tx_tail;
        bytes_buffered++;
    }
    spinlock_release(&sock->lock);

    if (bytes_buffered == 0) return 0;

    /* 
     * Move os bytes salvos no TX local diretamente para o RX do destino (dest_sock)
     * Tranca de forma segura o destino para evitar atropelamentos multicore.
     */
    uint32_t bytes_delivered = 0;

    spinlock_acquire(&dest_sock->lock);
    spinlock_acquire(&sock->lock);

    while (sock->tx_head != sock->tx_tail) 
    {
        uint32_t next_rx_tail = (dest_sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
        if (next_rx_tail == dest_sock->rx_head) break; // Buffer RX do destino encheu

        // Copia física de canais cruzados TX -> RX
        dest_sock->rx_buffer[dest_sock->rx_tail] = sock->tx_buffer[sock->tx_head];
        dest_sock->rx_tail = next_rx_tail;
        
        sock->tx_head = (sock->tx_head + 1) % SOCKET_BUFFER_SIZE;
        bytes_delivered++;
    }

    spinlock_release(&sock->lock);
    spinlock_release(&dest_sock->lock);

    return (long)bytes_delivered;
}

/**
 * @brief Captura e consome mensagens locais unicamente do seu rx_buffer de entrada.
 */
static long af_local_recvfrom(socket_t* sock, void* buf, unsigned long len, int flags, void* src_addr, unsigned long* addrlen) 
{
    (void)flags;
    if (!sock || !buf || len == 0 || sock->rx_buffer == NULL) return -1;

    uint8_t* dest = (uint8_t*)buf;
    uint32_t bytes_read = 0;

    /* EXTRAÇÃO EXCLUSIVA DO SEU PRÓPRIO BUFFER DE ENTRADA (RX) */
    spinlock_acquire(&sock->lock);

    while (sock->rx_head != sock->rx_tail && bytes_read < len) 
    {
        dest[bytes_read] = sock->rx_buffer[sock->rx_head];
        sock->rx_head = (sock->rx_head + 1) % SOCKET_BUFFER_SIZE;
        bytes_read++;
    }

    if (bytes_read > 0 && src_addr && addrlen && *addrlen > 0) 
    {
        socket_t* src_target = (sock->state == 1 && sock->peer) ? sock->peer : sock;
        unsigned long copy_len = (src_target->local_addr_len < *addrlen) ? 
                                  src_target->local_addr_len : *addrlen;
        
        memcpy(src_addr, src_target->local_addr, copy_len);
        *addrlen = src_target->local_addr_len;
    }

    spinlock_release(&sock->lock);
    return (long)bytes_read;
}

/* ============================================================================
 * EXPORTAÇÃO COMPLETA DA TABELA POLIMÓRFICA DO PROTOCOLO LOCAL
 * ============================================================================
 */
protocol_operations_t g_af_local_ops = {
    .bind     = af_local_bind,
    .connect  = af_local_connect,
    .sendto   = af_local_sendto,
    .recvfrom = af_local_recvfrom,
    .listen   = af_local_listen,
    .accept   = af_local_accept
};