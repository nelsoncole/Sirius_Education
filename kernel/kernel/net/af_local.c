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
 *  Modified Date: 01/11/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/socket.h>
#include <kernel/kernel/net/un.h>
#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/core/spinlock.h>
#include <kernel/klib.h>
#include <kernel/lib/string.h>
#include <kernel/kernel/sched/wait.h>
#include <kernel/kernel/sched/wakeup.h>

#define SOCKET_CREATED    0
#define SOCKET_CONNECTED  1
#define SOCKET_LISTENING  2
#define SOCKET_CONNECTING 3
#define SOCKET_CLOSING    4

static void af_local_lock_pair(socket_t* a, socket_t* b)
{
    if (a == b) {
        spinlock_acquire(&a->lock);
        return;
    }

    if ((uintptr_t)a < (uintptr_t)b) {
        spinlock_acquire(&a->lock);
        spinlock_acquire(&b->lock);
    } else {
        spinlock_acquire(&b->lock);
        spinlock_acquire(&a->lock);
    }
}

static void af_local_unlock_pair(socket_t* a, socket_t* b)
{
    if (a == b) {
        spinlock_release(&a->lock);
        return;
    }

    if ((uintptr_t)a < (uintptr_t)b) {
        spinlock_release(&b->lock);
        spinlock_release(&a->lock);
    } else {
        spinlock_release(&a->lock);
        spinlock_release(&b->lock);
    }
}

static int af_local_is_active(socket_t* sock)
{
    return sock &&
           sock->state != SOCKET_CREATED &&
           sock->state != SOCKET_CLOSING;
}

/**
 * Associa um endereço ao socket.
 */
static int af_local_bind(socket_t* sock, const void* addr,
                         unsigned long addrlen)
{
    if (!sock || !addr)
        return -1;

    if (sock->family != AF_LOCAL)
        return -1;

    if (addrlen < sizeof(struct sockaddr_un))
        return -1;

    if (addrlen > sizeof(sock->local_addr))
        return -1;

    const struct sockaddr_un* unaddr =
        (const struct sockaddr_un*)addr;

    if (unaddr->sun_family != AF_LOCAL)
        return -1;

    unsigned long path_len = 0;

    while (path_len < sizeof(unaddr->sun_path) &&
           unaddr->sun_path[path_len] != '\0') {
        path_len++;
    }

    if (path_len == 0 ||
        path_len == sizeof(unaddr->sun_path))
        return -1;

    spinlock_acquire(&sock->lock);

    if (sock->state == SOCKET_CLOSING ||
        sock->state == SOCKET_CONNECTED ||
        sock->state == SOCKET_CONNECTING ||
        sock->state == SOCKET_LISTENING) {
        spinlock_release(&sock->lock);
        return -1;
    }

    spinlock_release(&sock->lock);

    return socket_bind_address(
        sock,
        addr,
        sizeof(struct sockaddr_un)
    );
}

/**
 * Estabelece uma conexão local.
 */
static int af_local_connect(socket_t *sock, const void *addr,
                            unsigned long addrlen)
{
    if (!sock || !addr || addrlen == 0)
        return -1;

    socket_t *server_sock =
        socket_find_by_address(addr, addrlen, AF_LOCAL);

    if (!server_sock || server_sock == sock)
        return -1;

    for (;;)
    {
        spinlock_acquire(&sock->lock);

        while (sock->state == SOCKET_CONNECTED ||
               sock->state == SOCKET_CONNECTING)
        {
            wait(&sock->wait_queue, &sock->lock);
        }

        if (sock->state == SOCKET_CLOSING)
        {
            spinlock_release(&sock->lock);
            return -1;
        }

        spinlock_release(&sock->lock);

        af_local_lock_pair(sock, server_sock);

        if (sock->state == SOCKET_CONNECTED ||
            sock->state == SOCKET_CONNECTING)
        {
            af_local_unlock_pair(sock, server_sock);
            continue;
        }

        if (sock->state == SOCKET_CLOSING ||
            server_sock->state != SOCKET_LISTENING)
        {
            af_local_unlock_pair(sock, server_sock);
            return -1;
        }

        sock->state = SOCKET_CONNECTING;
        sock->next = server_sock->listen_queue;
        server_sock->listen_queue = sock;

        af_local_unlock_pair(sock, server_sock);

        wakeup(&server_sock->wait_queue);

        spinlock_acquire(&sock->lock);

        while (sock->state == SOCKET_CONNECTING)
        {
            wait(&sock->wait_queue, &sock->lock);
        }

        int connected = (sock->state == SOCKET_CONNECTED);

        spinlock_release(&sock->lock);

        return connected ? 0 : -1;
    }
}

/**
 * Aceita uma conexão pendente.
 */
static socket_t* af_local_accept(socket_t* sock)
{
    if (!sock)
        return NULL;

    spinlock_acquire(&sock->lock);

    while (sock->listen_queue == NULL &&
           sock->state == SOCKET_LISTENING) {
        wait(&sock->wait_queue, &sock->lock);
    }

    if (sock->state != SOCKET_LISTENING) {
        spinlock_release(&sock->lock);
        return NULL;
    }

    socket_t* client_sock = sock->listen_queue;

    if (!client_sock) {
        spinlock_release(&sock->lock);
        return NULL;
    }

    sock->listen_queue = client_sock->next;
    client_sock->next = NULL;

    spinlock_release(&sock->lock);

    socket_t* session_sock =
        socket_create(AF_LOCAL, sock->type, 0);

    if (!session_sock) {
        spinlock_acquire(&client_sock->lock);

        if (client_sock->state == SOCKET_CONNECTING) {
            client_sock->state = SOCKET_CREATED;
            client_sock->next = NULL;
            wakeup(&client_sock->wait_queue);
        }

        spinlock_release(&client_sock->lock);

        return NULL;
    }

    af_local_lock_pair(session_sock, client_sock);

    if (client_sock->state != SOCKET_CONNECTING ||
        session_sock->state == SOCKET_CLOSING) {

        af_local_unlock_pair(session_sock, client_sock);

        socket_close(session_sock);

        return NULL;
    }

    session_sock->peer = client_sock;
    client_sock->peer = session_sock;

    session_sock->state = SOCKET_CONNECTED;
    client_sock->state = SOCKET_CONNECTED;

    wakeup(&client_sock->wait_queue);

    af_local_unlock_pair(session_sock, client_sock);

    return session_sock;
}

/**
 * Coloca o socket em modo de escuta.
 */
static int af_local_listen(socket_t* sock, int backlog)
{
    (void)backlog;

    if (!sock)
        return -1;

    spinlock_acquire(&sock->lock);

    if (sock->state == SOCKET_CLOSING ||
        sock->state == SOCKET_CONNECTED ||
        sock->state == SOCKET_CONNECTING) {
        spinlock_release(&sock->lock);
        return -1;
    }

    sock->state = SOCKET_LISTENING;

    spinlock_release(&sock->lock);

    return 0;
}

/**
 * Envia dados para o socket de destino.
 */
static long af_local_sendto(socket_t* sock, const void* buf,
                            unsigned long len, int flags,
                            const void* dest_addr,
                            unsigned long addrlen)
{
    (void)flags;

    if (!sock || !buf || len == 0)
        return -1;

    socket_t* dest_sock = NULL;

    spinlock_acquire(&sock->lock);

    if (sock->state == SOCKET_CONNECTED && sock->peer) {
        dest_sock = sock->peer;
    }

    spinlock_release(&sock->lock);

    if (!dest_sock) {
        if (!dest_addr || addrlen == 0)
            return -1;

        dest_sock = socket_find_by_address(
            dest_addr,
            addrlen,
            AF_LOCAL
        );

        if (!dest_sock)
            return -1;
    }

    if (dest_sock == sock)
        return -1;

    spinlock_acquire(&dest_sock->lock);

    if (!af_local_is_active(dest_sock) ||
        !dest_sock->rx_buffer) {
        spinlock_release(&dest_sock->lock);
        return -1;
    }

    const uint8_t* src = (const uint8_t*)buf;

    uint32_t bytes_delivered = 0;

    for (unsigned long i = 0; i < len; i++) {

        uint32_t next_head =
            (dest_sock->rx_head + 1) % SOCKET_BUFFER_SIZE;

        if (next_head == dest_sock->rx_tail)
            break;

        dest_sock->rx_buffer[dest_sock->rx_head] = src[i];

        dest_sock->rx_head = next_head;

        bytes_delivered++;
    }

    if (bytes_delivered > 0)
        wakeup(&dest_sock->wait_queue);

    spinlock_release(&dest_sock->lock);

    return (long)bytes_delivered;
}

/**
 * Recebe dados do socket.
 */
static long af_local_recvfrom(socket_t* sock, void* buf,
                              unsigned long len, int flags,
                              void* src_addr,
                              unsigned long* addrlen)
{
    (void)flags;

    if (!sock || !buf || len == 0)
        return -1;

    spinlock_acquire(&sock->lock);

    if (!sock->rx_buffer) {
        spinlock_release(&sock->lock);
        return -1;
    }

    while (sock->rx_head == sock->rx_tail &&
           sock->state == SOCKET_CONNECTED) {
        wait(&sock->wait_queue, &sock->lock);
    }

    if (sock->rx_head == sock->rx_tail &&
        sock->state != SOCKET_CONNECTED) {
        spinlock_release(&sock->lock);
        return 0;
    }

    uint8_t* dest = (uint8_t*)buf;

    uint32_t bytes_read = 0;

    while (sock->rx_head != sock->rx_tail &&
           bytes_read < len) {

        dest[bytes_read] =
            sock->rx_buffer[sock->rx_tail];

        sock->rx_tail =
            (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;

        bytes_read++;
    }

    socket_t* src_target = NULL;

    if (bytes_read > 0 &&
        src_addr &&
        addrlen &&
        *addrlen > 0) {
        src_target = sock->peer;
    }

    spinlock_release(&sock->lock);

    if (src_target) {
        spinlock_acquire(&src_target->lock);

        unsigned long copy_len =
            src_target->local_addr_len < *addrlen
            ? src_target->local_addr_len
            : *addrlen;

        memcpy(
            src_addr,
            src_target->local_addr,
            copy_len
        );

        *addrlen = src_target->local_addr_len;

        spinlock_release(&src_target->lock);
    }

    return (long)bytes_read;
}

/**
 * Fecha e liberta a ligação local.
 */
static int af_local_release(socket_t* sock)
{
    if (!sock)
        return -1;

    spinlock_acquire(&sock->lock);

    if (sock->state == SOCKET_CLOSING) {
        spinlock_release(&sock->lock);
        return 0;
    }

    sock->state = SOCKET_CLOSING;

    socket_t* pending = sock->listen_queue;
    socket_t* peer_sock = sock->peer;

    sock->listen_queue = NULL;
    sock->peer = NULL;

    spinlock_release(&sock->lock);

    wakeup(&sock->wait_queue);

    while (pending) {

        socket_t* next = pending->next;

        spinlock_acquire(&pending->lock);

        pending->next = NULL;

        if (pending->state == SOCKET_CONNECTING) {
            pending->state = SOCKET_CREATED;
        }

        wakeup(&pending->wait_queue);

        spinlock_release(&pending->lock);

        pending = next;
    }

    if (peer_sock) {

        spinlock_acquire(&peer_sock->lock);

        if (peer_sock->peer == sock) {

            peer_sock->peer = NULL;

            if (peer_sock->state == SOCKET_CONNECTED ||
                peer_sock->state == SOCKET_CONNECTING) {
                peer_sock->state = SOCKET_CREATED;
            }

            wakeup(&peer_sock->wait_queue);
        }

        spinlock_release(&peer_sock->lock);
    }

    return 0;
}

protocol_operations_t g_af_local_ops = {
    .bind     = af_local_bind,
    .connect  = af_local_connect,
    .sendto   = af_local_sendto,
    .recvfrom = af_local_recvfrom,
    .listen   = af_local_listen,
    .accept   = af_local_accept,
    .release  = af_local_release
};