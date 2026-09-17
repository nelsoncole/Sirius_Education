/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: socket.c
 *    Description: Implementação das rotinas nativas centrais do subsistema de 
 *                 Sockets POSIX. Atua como barramento polimórfico genérico,
 *                 delegando a lógica para cada família de protocolo e integrando
 *                 os descritores com a árvore de ficheiros do VFS.
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
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>
#include <kernel/lib/string.h>
#include <kernel/kernel/core/spinlock.h>

// Lista encadeada global de sockets em modo LISTEN (mapeamento de servidores)
socket_t* g_bound_sockets_head = NULL;

// Declarações das tabelas de operações externas das famílias de protocolos
extern protocol_operations_t g_af_local_ops;
extern protocol_operations_t g_af_inet_ops;
extern protocol_operations_t g_pf_packet_ops;

// Spinlock global para a consistência da árvore de portas/caminhos locais
spinlock_t g_socket_list_lock = { SPINLOCK_RELEASED };

/**
 * @brief Inicializa as estruturas globais e o subsistema de sockets do Kernel.
 */
void init_socket(void) 
{
    g_bound_sockets_head = NULL;
    spin_lock_init(&g_socket_list_lock);
    kprintf("[NET] Subsistema de Sockets POSIX inicializado com sucesso.\n");
}

/* ============================================================================
 * PONTE POLIMÓRFICA DE OPERAÇÕES DO VFS (Alinhada com vfs_operations_t)
 * ============================================================================
 */

static int socket_vfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) 
{
    /* Silencia o parâmetro nativo exigido pelo VFS */
    (void)offset;

    if (!node || !node->private_data || !buffer || size == 0) return -1;

    socket_t* sock = (socket_t*)node->private_data;

    /* 
     * POLIMORFISMO ABSOLUTO:
     * O VFS delega 100% da transmissão para o driver do protocolo ativo.
     * Passa NULL e 0 no endereço pois assume-se um fluxo pré-conectado ou anónimo.
     */
    if (sock->proto_ops && sock->proto_ops->sendto) 
    {
        return (int)sock->proto_ops->sendto(sock, buffer, size, 0, NULL, 0);
    }

    return -1; 
}

static int socket_vfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) 
{
    /* Silencia o parâmetro nativo exigido pelo VFS */
    (void)offset;

    if (!node || !node->private_data || !buffer || size == 0) return -1;

    socket_t* sock = (socket_t*)node->private_data;

    /* 
     * POLIMORFISMO ABSOLUTO:
     * O VFS delega 100% da leitura para o driver do protocolo ativo.
     */
    if (sock->proto_ops && sock->proto_ops->recvfrom) 
    {
        return (int)sock->proto_ops->recvfrom(sock, buffer, size, 0, NULL, NULL);
    }

    return -1; 
}


static int socket_vfs_close(vfs_node_t* node) 
{
    if (!node || !node->private_data) return -1;

    socket_t* sock = (socket_t*)node->private_data;

    if (sock->peer) {
        sock->peer->state = 0; 
        sock->peer->peer = NULL;
    }

    /* LIMPEZA ATÓMICA DE AMBOS OS BUFFERES NO HEAP */
    if (sock->rx_buffer) kfree(sock->rx_buffer);
    if (sock->tx_buffer) kfree(sock->tx_buffer);

    kfree(sock);
    node->private_data = NULL;
    return 0; 
}

static vfs_operations_t g_socket_vfs_ops = {
    .write    = socket_vfs_write,
    .read     = socket_vfs_read,
    .open     = NULL,
    .close    = socket_vfs_close,
    .flush    = NULL,
    .finddir  = NULL,
    .readdir  = NULL,
    .mkdir    = NULL,
    .create   = NULL,
    .unlink   = NULL,
    .rmdir    = NULL,
    .stat     = NULL,
    .chmod    = NULL,
    .rename   = NULL
};

/* ============================================================================
 * INTERFACE DE CHAMADAS DE SISTEMA DO UTILIZADOR (CONTRAPARTIDA RING 0)
 * ============================================================================
 */

int socket(int family, int type, int protocol) 
{
    (void)protocol;
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;

    if (family < AF_UNSPEC || family > PF_PACKET) return -1;

    socket_t* sock = (socket_t*)kmalloc(sizeof(socket_t));
    vfs_node_t* vnode = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));

    if (!sock || !vnode) return -1;

    memset(sock, 0, sizeof(socket_t));
    memset(vnode, 0, sizeof(vfs_node_t));

    sock->family = family;
    sock->type = type;
    sock->state = 0; 
    sock->lock.lock = SPINLOCK_RELEASED; 

    // Vinculação e inicialização por driver de protocolo com 2 buffers
    if (family == AF_LOCAL) {
        sock->proto_ops = &g_af_local_ops;
        
        // Aloca nativamente a Fila de Entrada (RX)
        sock->rx_buffer = (uint8_t*)kmalloc(SOCKET_BUFFER_SIZE);
        // Aloca nativamente a Fila de Saída (TX)
        sock->tx_buffer = (uint8_t*)kmalloc(SOCKET_BUFFER_SIZE);
        
        if (!sock->rx_buffer || !sock->tx_buffer) {
            if (sock->rx_buffer) kfree(sock->rx_buffer);
            if (sock->tx_buffer) kfree(sock->tx_buffer);
            kfree(sock);
            kfree(vnode);
            return -1; // Out of Memory no Kernel
        }
    } else if (family == AF_INET) {
        sock->proto_ops = &g_af_inet_ops;
        sock->rx_buffer = NULL;
        sock->tx_buffer = NULL; // Controlado por anéis DMA da placa
    } else if (family == PF_PACKET) {
        sock->proto_ops = &g_pf_packet_ops;
        sock->rx_buffer = NULL;
        sock->tx_buffer = NULL; // Injeta direto no cabo físico
    }

    vnode->flags = VFS_CHAR_DEV; 
    vnode->private_data = sock;   
    vnode->ops = &g_socket_vfs_ops;

    int fd = -1;
    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) 
    {
        if (proc->file_descriptor_table[i] == NULL) 
        {
            vfs_file_t* file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
            if (!file) return -1;

            file->node = vnode;
            file->flags = VFS_MODE_READ | VFS_MODE_WRITE;
            file->offset = 0;

            proc->file_descriptor_table[i] = file;
            fd = i;
            break;
        }
    }

    return fd;
}

int bind(int fd, const void* addr, unsigned long addrlen) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !proc->file_descriptor_table[fd]) return -1;

    vfs_file_t* file = proc->file_descriptor_table[fd];
    socket_t* sock = (socket_t*)file->node->private_data;

    if (!sock) return -1;

    // Delegação puramente polimórfica para o protocolo
    if (sock->proto_ops && sock->proto_ops->bind) {
        return sock->proto_ops->bind(sock, addr, addrlen);
    }

    return -1;
}

int listen(int fd, int backlog) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !proc->file_descriptor_table[fd]) return -1;

    vfs_file_t* file = proc->file_descriptor_table[fd];
    socket_t* sock = (socket_t*)file->node->private_data;

    if (sock && sock->proto_ops && sock->proto_ops->listen) {
        return sock->proto_ops->listen(sock, backlog);
    }

    return -1;
}

int accept(int fd, void* addr, unsigned long* addrlen) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !proc->file_descriptor_table[fd]) return -1;

    vfs_file_t* file = proc->file_descriptor_table[fd];
    socket_t* sock = (socket_t*)file->node->private_data;

    if (sock && sock->proto_ops && sock->proto_ops->accept) {
        // O accept polimórfico trata a fila e cria a amarração interna
        socket_t* client = sock->proto_ops->accept(sock);
        if (!client) return -1;
        
        (void)addr; (void)addrlen; // Suprime avisos se stubs omitirem dados
        return 0;
    }

    return -1;
}

int connect(int fd, const void* addr, unsigned long addrlen) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !proc->file_descriptor_table[fd]) return -1;

    vfs_file_t* file = proc->file_descriptor_table[fd];
    socket_t* sock = (socket_t*)file->node->private_data;

    if (sock && sock->proto_ops && sock->proto_ops->connect) {
        return sock->proto_ops->connect(sock, addr, addrlen);
    }

    return -1;
}

long sendto(int fd, const void* buf, unsigned long len, int flags, const void* dest_addr, unsigned long addrlen) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !proc->file_descriptor_table[fd]) return -1;

    vfs_file_t* file = proc->file_descriptor_table[fd];
    socket_t* sock = (socket_t*)file->node->private_data;

    if (sock && sock->proto_ops && sock->proto_ops->sendto) {
        return sock->proto_ops->sendto(sock, buf, len, flags, dest_addr, addrlen);
    }

    return -1;
}

long recvfrom(int fd, void* buf, unsigned long len, int flags, void* src_addr, unsigned long* addrlen) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !proc->file_descriptor_table[fd])
        return -1;

    vfs_file_t *file = proc->file_descriptor_table[fd];
    socket_t *sock = (socket_t *)file->node->private_data;

    if (sock && sock->proto_ops && sock->proto_ops->recvfrom)
    {
        return sock->proto_ops->recvfrom(sock, buf, len, flags, src_addr, addrlen);
    }

    return -1;
}

long send(int fd, const void *buf, unsigned long len, int flags) { 
    return sendto(fd, buf, len, flags, NULL, 0); 
}

long recv(int fd, void *buf, unsigned long len, int flags) { 
    return recvfrom(fd, buf, len, flags, NULL, NULL); 
}

int shutdown(int fd, int how)
{
    cpu_data_block_t *cpu = get_current_cpu();
    process_t *proc = cpu->current_thread->owner;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !proc->file_descriptor_table[fd])
        return -1;

    vfs_file_t *file = proc->file_descriptor_table[fd];

    if (file->node->flags != VFS_CHAR_DEV)
        return -1;
    switch (how)
    {
    case 0:
        file->flags &= ~VFS_MODE_READ;
        break;
    case 1:
        file->flags &= ~VFS_MODE_WRITE;
        break;
    case 2:
        file->flags &= ~(VFS_MODE_READ | VFS_MODE_WRITE);
        break;
    default:
        return -1;
    }
    return 0;
}