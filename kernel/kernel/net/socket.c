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
 *  Modified Date: 21/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/net.h>
#include <kernel/kernel/net/socket.h>
#include <kernel/kernel/sched/process.h>
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

/**
 * @brief Aloca um endereço nominal de forma atómica após varredura contra colisões.
 */
int socket_bind_address(socket_t* sock, const void* addr, unsigned long addrlen)
{
    spin_lock(&g_socket_list_lock);

    socket_t* check = g_bound_sockets_head;
    while (check != NULL) 
    {
        if (check->family == sock->family && check->local_addr_len == addrlen && 
            memcmp(check->local_addr, addr, addrlen) == 0) 
        {
            spin_unlock(&g_socket_list_lock);
            return -2; /* EADDRINUSE */
        }
        check = check->next;
    }

    /* Grava os dados na estrutura física */
    memcpy(sock->local_addr, addr, addrlen);
    sock->local_addr_len = addrlen;

    /* Encadeia na árvore global do sistema */
    sock->next = g_bound_sockets_head;
    g_bound_sockets_head = sock;

    spin_unlock(&g_socket_list_lock);
    return 0; /* Sucesso */
}

/**
 * @brief Busca centralizada de sockets baseada no endereço nominal e na família.
 */
socket_t* socket_find_by_address(const void* addr, unsigned long addrlen, int family)
{
    spin_lock(&g_socket_list_lock);

    socket_t* curr = g_bound_sockets_head;
    while (curr != NULL) 
    {
        if (curr->family == family && curr->local_addr_len == addrlen && 
            memcmp(curr->local_addr, addr, addrlen) == 0) 
        {
            spin_unlock(&g_socket_list_lock);
            return curr;
        }
        curr = curr->next;
    }

    spin_unlock(&g_socket_list_lock);
    return NULL;
}

/**
 * @brief socket_find_by_port - Localiza um socket AF_INET ativo atrelado a uma porta específica.
 * 
 * @param port           Porta de destino (em Network Byte Order / Big-Endian).
 * @param protocol_type  Tipo lógico (SOCK_STREAM para TCP ou SOCK_DGRAM para UDP).
 * @return Ponteiro para o socket_t correspondente, ou NULL se nenhum estiver à escuta.
 */
socket_t* socket_find_by_port(uint16_t port, int protocol_type) 
{
    spin_lock(&g_socket_list_lock);

    socket_t* curr = g_bound_sockets_head;

    while (curr != NULL) 
    {
        /* Filtra estritamente pela família IPv4 e pelo tipo de transporte correto */
        if (curr->family == AF_INET && curr->type == protocol_type) 
        {
            /* Faz o cast seguro do buffer genérico local_addr para a estrutura da internet */
            struct sockaddr_in* local_sin = (struct sockaddr_in*)curr->local_addr;
            
            /* Compara a porta binária bruta de rede (Big-Endian) sem conversões redundantes */
            if (local_sin->sin_port == port) 
            {
                spin_unlock(&g_socket_list_lock);
                return curr; /* Retorna a sessão ativa do socket */
            }
        }
        curr = curr->next; /* Avança na lista encadeada */
    }

    spin_unlock(&g_socket_list_lock);
    return NULL; /* Porta fechada */
}

/**
 * @brief Adiciona de forma segura um socket cliente à fila de conexões pendentes do accept.
 */
int socket_add_listen_queue(socket_t* server, socket_t* client)
{
    spin_lock(&server->lock);

    client->next = server->listen_queue;
    server->listen_queue = client;

    spin_unlock(&server->lock);
    return 0;
}

/* ============================================================================
 * PONTE POLIMÓRFICA DE OPERAÇÕES DO VFS (Alinhada com vfs_operations_t)
 * ============================================================================
 */
static int socket_auto_bind_inet(socket_t* sock);
static int socket_vfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) 
{
    /* Silencia o parâmetro nativo exigido pelo VFS */
    (void)offset;

    if (!node || !node->private_data || !buffer || size == 0) return -1;

    socket_t* sock = (socket_t*)node->private_data;

    /* AUTO-BIND AUTOMÁTICO PARA IPV4 */
    if (sock->family == AF_INET && sock->local_addr_len == 0) 
    {
        if (socket_auto_bind_inet(sock) < 0) 
        {
            return -2; /* Falha ao alocar porta efêmera */
        }
    }

    /* 
     * POLIMORFISMO ABSOLUTO:
     * O VFS delega 100% da transmissão para o driver do protocolo ativo.
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

    /* REMOVE DA LISTA GLOBAL DO KERNEL SE ELE ESTIVESSE REGISTADO VIA BIND */
    spin_lock(&g_socket_list_lock);
    socket_t* curr = g_bound_sockets_head;
    socket_t* prev = NULL;
    while (curr != NULL) {
        if (curr == sock) {
            if (prev == NULL) g_bound_sockets_head = curr->next;
            else prev->next = curr->next;
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    spin_unlock(&g_socket_list_lock);

    if (sock->peer) {
        sock->peer->state = 0; 
        sock->peer->peer = NULL;
    }

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

socket_t* socket_create(int family, int type, int protocol)
{
    (void)protocol;

    if (family < AF_UNSPEC || family > PF_PACKET) return NULL;

    socket_t* sock = (socket_t*)kmalloc(sizeof(socket_t));
    if (!sock) return NULL;

    memset(sock, 0, sizeof(socket_t));

    sock->family = family;
    sock->type = type;
    sock->state = 0; 
    spin_lock_init(&sock->lock);

    /* ASSOCIAÇÃO POLIMÓRFICA DE OPERAÇÕES */
    if (family == AF_LOCAL) {
        sock->proto_ops = &g_af_local_ops;
    }
    else if (family == AF_INET) {
        sock->proto_ops = &g_af_inet_ops;
    }
    else if (family == PF_PACKET) {
        sock->proto_ops = &g_pf_packet_ops;
    }

    /* ALOCAÇÃO DE BUFFER DE RECEÇÃO (Comum a todas as famílias implementadas) */
    sock->rx_buffer = (uint8_t*)kmalloc(SOCKET_BUFFER_SIZE);
    sock->tx_buffer = NULL; /* Transmissão Direta / Zero-Copy para todas as famílias */

    /* Salvaguarda Unificada de falta de memória (OOM) */
    if (!sock->rx_buffer) {
        kfree(sock);
        return NULL;
    }

    return sock;
}

int socket_close(socket_t* sock) 
{
    if (!sock) return -1;

    /* REMOVE DA LISTA GLOBAL DO KERNEL SE ELE ESTIVESSE REGISTADO VIA BIND */
    spin_lock(&g_socket_list_lock);
    socket_t* curr = g_bound_sockets_head;
    socket_t* prev = NULL;
    while (curr != NULL) {
        if (curr == sock) {
            if (prev == NULL) g_bound_sockets_head = curr->next;
            else prev->next = curr->next;
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    spin_unlock(&g_socket_list_lock);

    if (sock->peer) {
        sock->peer->state = 0; 
        sock->peer->peer = NULL;
    }

    if (sock->rx_buffer) kfree(sock->rx_buffer);
    if (sock->tx_buffer) kfree(sock->tx_buffer);

    kfree(sock);
    return 0; 
}

int socket(int family, int type, int protocol) 
{
    (void)protocol;
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;

    if (family < AF_UNSPEC || family > PF_PACKET) return -1;

    socket_t* sock = socket_create(family, type, protocol);
    vfs_node_t* vnode = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));

    if (!sock || !vnode) {
        if (sock) socket_close(sock);
        if (vnode) kfree(vnode);
        return -1;
    }

    memset(vnode, 0, sizeof(vfs_node_t));

    vnode->flags = VFS_CHAR_DEV; 
    vnode->private_data = sock;   
    vnode->ops = &g_socket_vfs_ops;

    /* Encadeia o novo ficheiro virtual na tabela de FDs do processo */
    int fd = -1;
    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) 
    {
        if (proc->file_descriptor_table[i] == NULL) 
        {
            vfs_file_t* file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
            if (!file) {
                socket_vfs_close(vnode);
                kfree(vnode);
                return -1;
            }
            file->node = vnode;
            file->flags = VFS_MODE_READ | VFS_MODE_WRITE;
            file->offset = 0;

            proc->file_descriptor_table[i] = file;
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        socket_vfs_close(vnode);
        kfree(vnode);
    }

    return fd;
}

/**
 * @brief Tenta alocar uma porta efêmera aleatória/sequencial livre para Sockets AF_INET.
 * @note Deve ser chamada ANTES do envio de dados se local_addr_len for 0.
 * @return 0 em caso de sucesso, ou -2 (EADDRINUSE) se não houver portas livres.
 */
static int socket_auto_bind_inet(socket_t* sock)
{
    struct sockaddr_in auto_sin;
    int port_found = 0;

    memset(&auto_sin, 0, sizeof(struct sockaddr_in));
    auto_sin.sin_family = AF_INET;
    // Vincula explicitamente a qualquer interface (INADDR_ANY)
    auto_sin.sin_addr.s_addr = 0; 

    spin_lock(&g_socket_list_lock);

    // Procura por uma porta livre no intervalo efêmero
    for (uint32_t port = EPHEMERAL_PORT_START; port <= EPHEMERAL_PORT_END; port++) 
    {
        // Converte para Big-Endian (Network Byte Order) para correspondência estrita
        uint16_t net_port = (uint16_t)(((port & 0xFF) << 8) | ((port & 0xFF00) >> 8));
        int in_use = 0;

        socket_t* check = g_bound_sockets_head;
        while (check != NULL) 
        {
            if (check->family == AF_INET && check->type == sock->type) 
            {
                struct sockaddr_in* check_sin = (struct sockaddr_in*)check->local_addr;
                if (check_sin->sin_port == net_port) 
                {
                    in_use = 1;
                    break;
                }
            }
            check = check->next;
        }

        if (!in_use) 
        {
            auto_sin.sin_port = net_port;
            port_found = 1;
            break;
        }
    }

    if (!port_found) 
    {
        spin_unlock(&g_socket_list_lock);
        return -2; /* EADDRINUSE */
    }

    memcpy(sock->local_addr, &auto_sin, sizeof(struct sockaddr_in));
    sock->local_addr_len = sizeof(struct sockaddr_in);

    sock->next = g_bound_sockets_head;
    g_bound_sockets_head = sock;

    spin_unlock(&g_socket_list_lock);
    return 0; /* Sucesso */
}


int getsockname(vfs_node_t* node, void* addr, unsigned long* addrlen)
{
    if (!node || !node->private_data || !addr || !addrlen) return -1;

    socket_t* sock = (socket_t*)node->private_data;

    // Protege o acesso às propriedades de endereço do socket
    spin_lock(&g_socket_list_lock);

    unsigned long len_to_copy = sock->local_addr_len;
    if (*addrlen < len_to_copy) 
    {
        len_to_copy = *addrlen;
    }

    memcpy(addr, sock->local_addr, len_to_copy);
    *addrlen = sock->local_addr_len;

    spin_unlock(&g_socket_list_lock);
    return 0;
}

int bind(int fd, const void* addr, unsigned long addrlen) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !proc->file_descriptor_table[fd]) return -1;

    vfs_file_t* file = proc->file_descriptor_table[fd];
    socket_t* sock = (socket_t*)file->node->private_data;
    if (!sock) return -1;

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
    (void)addr; (void)addrlen;
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !proc->file_descriptor_table[fd]) return -1;

    vfs_file_t* file = proc->file_descriptor_table[fd];
    socket_t* sock = (socket_t*)file->node->private_data;

    if (sock && sock->proto_ops && sock->proto_ops->accept) {
        socket_t* client = sock->proto_ops->accept(sock);
        if (!client) return -1;
        
        /* 
         * No futuro, clone o nó VFS e coloque o novo 'client' atrelado 
         * a um novo FD para retornar para a aplicação Ring 3!
         */
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
    if (!file || !file->node || !file->node->private_data) return -1; // Validação do VFS

    socket_t* sock = (socket_t*)file->node->private_data;
    if (!sock) return -1;

    /* AUTO-BIND AUTOMÁTICO PARA IPV4 */
    if (sock->family == AF_INET && sock->local_addr_len == 0) 
    {
        if (socket_auto_bind_inet(sock) < 0) 
        {
            return -2; /* Falha ao alocar porta efêmera */
        }
    }

    if (sock->proto_ops && sock->proto_ops->connect) {
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
    if (!sock) return -1;

    /* AUTO-BIND AUTOMÁTICO PARA IPV4 */
    if (sock->family == AF_INET && sock->local_addr_len == 0) 
    {
        if (socket_auto_bind_inet(sock) < 0) 
        {
            return -2; /* Falha ao alocar porta efêmera */
        }
    }

    if (sock->proto_ops && sock->proto_ops->sendto) {
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