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
 *  Modified Date: 18/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/socket.h>
#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/core/spinlock.h>
#include <kernel/klib.h>
#include <kernel/lib/string.h>

/**
 * @brief Associa uma identidade nominal (endereço/caminho) ao socket local.
 */
static int af_local_bind(socket_t* sock, const void* addr, unsigned long addrlen) 
{
    if (!sock || !addr || addrlen == 0 || addrlen > 256) return -1;

    /* Delega a validação de duplicados e o encadeamento global para o barramento do socket.c */
    int res = socket_bind_address(sock, addr, addrlen);
    if (res < 0) {
        kprintf("[AF_LOCAL] Erro: Endereco ja em uso ou falha no bind.\n");
        return res;
    }

    return 0;
}

/**
 * @brief Solicita uma conexão atómica ponto-a-ponto com um servidor local.
 */
static int af_local_connect(socket_t* sock, const void* addr, unsigned long addrlen) 
{
    if (!sock || !addr || addrlen == 0) return -1;

    /* Procura o socket do servidor na lista global gerenciada de forma segura */
    socket_t* server_sock = socket_find_by_address(addr, addrlen, AF_LOCAL);

    if (!server_sock || server_sock->state != 2) 
    {
        return -1; // Connection refused
    }

    /* Adiciona o socket cliente na fila de escuta (listen_queue) do servidor de forma atómica */
    socket_add_listen_queue(server_sock, sock);

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

    /* Protege a extração da fila local usando o lock do próprio socket */
    spinlock_acquire(&sock->lock);
    socket_t* client_sock = sock->listen_queue;
    if (client_sock) {
        sock->listen_queue = client_sock->next;
    }
    spinlock_release(&sock->lock);

    if (!client_sock) return NULL;

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
    if (!sock || !buf || len == 0) return -1;

    socket_t* dest_sock = NULL;

    /* 1. Resolução do alvo de destino */
    if (sock->state == 1 && sock->peer) 
    {
        dest_sock = sock->peer;
    }
    else 
    {
        if (!dest_addr || addrlen == 0) return -1;

        /* Procura o alvo estático na lista global gerenciada centralizadamente (ex: portas AF_LOCAL) */
        dest_sock = socket_find_by_address(dest_addr, addrlen, AF_LOCAL);
        if (!dest_sock || dest_sock->rx_buffer == NULL) return -1;
    }

    uint8_t* src = (uint8_t*)buf;
    uint32_t bytes_delivered = 0;

    /* 2. PROTEÇÃO SMP/MULTICORE ANTIDEADLOCK: Tranca APENAS o buffer do receptor!
       O remetente não precisa de trancar o seu próprio lock, pois lê direto do 'buf' da aplicação. */
    spinlock_acquire(&dest_sock->lock);

    /* 3. INJEÇÃO DIRETA NO RING BUFFER DO DESTINO */
    for (uint32_t i = 0; i < len; i++) 
    {
        /* Calcula a próxima cabeça RX do receptor de forma circular */
        uint32_t next_rx_head = (dest_sock->rx_head + 1) % SOCKET_BUFFER_SIZE;
        
        /* Overflow Protection: Verifica se o buffer do destino encheu */
        if (next_rx_head == dest_sock->rx_tail) 
        {
            break; 
        }

        /* Insere no rx_head e avança o rx_head */
        dest_sock->rx_buffer[dest_sock->rx_head] = src[i];
        dest_sock->rx_head = next_rx_head;
        bytes_delivered++;
    }

    spinlock_release(&dest_sock->lock);

    /* 
     * NOTA DE NOTIFICAÇÃO: Se entregou bytes com sucesso e possui um mecanismo de 
     * sinalização, pode disparar um wakeup na fila do dest_sock aqui para tirar a 
     * thread recetora do estado passivo "hlt" que configurámos no af_local_recvfrom.
     */

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

    /* Proteção atómica para o socket local (IPC) */
    spinlock_acquire(&sock->lock);

    /* 
     * 1. BLOQUEIO SEGURO LOCAL: 
     * Se o buffer estiver vazio, mas o peer local ainda estiver conectado (estado == 1),
     * a thread aguarda passivamente a chegada de dados enviados pelo outro processo.
     */
    while (sock->rx_head == sock->rx_tail && sock->state == 1) 
    {
        spinlock_release(&sock->lock);
        
        /* Cede o CPU de forma passiva (ou chame scheduler_yield()) */
        __asm__ __volatile__("hlt"); 
        
        spinlock_acquire(&sock->lock);
    }

    /* 
     * 2. TRATAMENTO DE EOF (Fim de Ficheiro POSIX):
     * Se o loop quebrou porque a conexão caiu (peer fechou o socket) e o buffer 
     * continua vazio, retorna 0 indicando desconexão limpa.
     */
    if (sock->rx_head == sock->rx_tail && sock->state != 1)
    {
        spinlock_release(&sock->lock);
        return 0; 
    }

    /* 
     * 3. CORREÇÃO DOS ÍNDICES (Consumo FIFO): 
     * Lê a partir de 'rx_tail' e avança o 'rx_tail' de forma circular.
     */
    while (sock->rx_head != sock->rx_tail && bytes_read < len) 
    {
        dest[bytes_read] = sock->rx_buffer[sock->rx_tail];
        sock->rx_tail = (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
        bytes_read++;
    }

    /* 4. Mapeamento simétrico do endereço do remetente local (AF_UNIX path) */
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

protocol_operations_t g_af_local_ops = {
    .bind     = af_local_bind,
    .connect  = af_local_connect,
    .sendto   = af_local_sendto,
    .recvfrom = af_local_recvfrom,
    .listen   = af_local_listen,
    .accept   = af_local_accept
};