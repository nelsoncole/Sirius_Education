/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pf_packet.c
 *    Description: Driver polimórfico completo para a família de pacotes 
 *                 brutos de baixo nível (PF_PACKET). Conecta o Ring 3 diretamente
 *                 aos controladores físicos de placas de rede (Link Layer).
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

/**
 * @brief Estrutura POSIX sockaddr_ll para endereçamento físico Low-Level (Link Layer).
 */
struct sockaddr_ll {
    uint16_t sll_family;   /* Sempre AF_PACKET / PF_PACKET */
    uint16_t sll_protocol; /* Protocolo físico em Network Byte Order (ex: EtherType IP ou ARP) */
    int32_t  sll_ifindex;  /* Índice numérico identificador da placa de rede (Interface Index) */
    uint16_t sll_hatype;   /* Tipo de hardware de cabeçalho */
    uint8_t  sll_pkttype;  /* Tipo de pacote */
    uint8_t  sll_halen;    /* Comprimento do endereço físico (ex: MAC = 6 bytes) */
    uint8_t  sll_addr[8];  /* Endereço de hardware físico real (Endereço MAC) */
};

/**
 * @brief Associa o Raw Socket a uma placa de rede específica (Interface Index).
 */
static int pf_packet_bind(socket_t* sock, const void* addr, unsigned long addrlen) 
{
    if (!sock || !addr || addrlen < sizeof(struct sockaddr_ll)) return -1;

    struct sockaddr_ll* sll = (struct sockaddr_ll*)addr;
    if (sll->sll_family != AF_PACKET) return -2;

    /* Delega a vinculação de identificadores locais de forma atómica para o socket.c */
    int res = socket_bind_address(sock, addr, sizeof(struct sockaddr_ll));
    if (res < 0) 
    {
        kprintf("[PF_PACKET] Erro: Falha ao registar interface ou vinculo ja existente.\n");
        return res;
    }

    //kprintf("[PF_PACKET] Bind efetuado. Socket acoplado a interface fisica de index %d.\n", sll->sll_ifindex);
    return 0; 
}

/**
 * @brief Sockets de pacotes brutos operam sem conexão. Esta chamada é inválida.
 */
static int pf_packet_connect(socket_t* sock, const void* addr, unsigned long addrlen) 
{
    (void)sock; (void)addr; (void)addrlen;
    kprintf("[PF_PACKET] Erro: Operacao 'connect' nao suportada para pacotes brutos.\n");
    return -1; /* EOPNOTSUPP */
}

/**
 * @brief Sockets de pacotes brutos não aceitam conexões passivas. Chamada inválida.
 */
static int pf_packet_listen(socket_t* sock, int backlog) 
{
    (void)sock; (void)backlog;
    kprintf("[PF_PACKET] Erro: Operacao 'listen' nao suportada para pacotes brutos.\n");
    return -1; /* EOPNOTSUPP */
}

/**
 * @brief Sockets de pacotes brutos não realizam handshakes. Chamada inválida.
 */
static socket_t* pf_packet_accept(socket_t* sock) 
{
    (void)sock;
    kprintf("[PF_PACKET] Erro: Operacao 'accept' nao suportada para pacotes brutos.\n");
    return NULL;
}

/**
 * @brief Injeta um frame Ethernet bruto diretamente na fila de transmissão do hardware.
 */
static long pf_packet_sendto(socket_t* sock, const void* buf, unsigned long len, int flags, const void* dest_addr, unsigned long addrlen) 
{
    (void)flags; (void)dest_addr; (void)addrlen;
    if (!sock || !buf || len == 0) return -1;

    /* Validação física: O frame completo (Cabeçalho MAC + Payload) não deve estourar o cabo */
    if (len > 1514) 
    {
        kprintf("[PF_PACKET Error] Frame bruto excede o tamanho limite físico Ethernet (1514 bytes).\n");
        return -2;
    }

    kprintf("[PF_PACKET] A injetar frame Ethernet bruto de %d bytes diretamente no hardware...\n", len);

    /* 
     * INJEÇÃO COESA DIRECTA (Zero-Copy):
     * O buffer passado pelo Ring 3 já contém a estrutura binária completa:
     * [MAC Destino (6b)] [MAC Origem (6b)] [EtherType (2b)] [Dados IP/ARP/etc...]
     */

    int res = net_driver_transmit(buf, len);
    if (res < 0) return -3;

    return (long)len; 
}

/**
 * @brief Captura frames Ethernet brutos recebidos pela placa (Modo Sniffing/Promíscuo).
 */
static long pf_packet_recvfrom(socket_t* sock, void* buf, unsigned long len, int flags, void* src_addr, unsigned long* addrlen) 
{
    (void)flags;
    if (!sock || !buf || len == 0 || sock->rx_buffer == NULL) return -1;

    uint8_t* dest = (uint8_t*)buf;
    uint32_t bytes_read = 0;

    /* Extração e consumo seguro a partir do Ring Buffer local da sessão do socket */
    spinlock_acquire(&sock->lock);

    /* 
     * 1. BLOQUEIO SEGURO RAW PACKET: 
     * Se o buffer estiver completamente vazio, a thread em Ring 3 cede o CPU 
     * passivamente até que a 'network_rx_thread' capture um frame do cabo e avance o rx_head.
     */
    while (sock->rx_head == sock->rx_tail) 
    {
        spinlock_release(&sock->lock);
        
        /* Coloca o core local em repouso passivo (ou chame scheduler_yield()) */
        __asm__ __volatile__("hlt"); 
        
        spinlock_acquire(&sock->lock);
    }

    /* 
     * FILTRO RAW PACKET: Captura o cabeçalho virtual de metadados inserido pelo packet_input
     * para preservar o tamanho exato de cada frame Ethernet individual capturado.
     */

    /* Passo A: Recolhe o tamanho em bytes do frame guardado (4 bytes) */
    uint32_t frame_len = 0;
    uint8_t* len_ptr = (uint8_t*)&frame_len;
    for (uint32_t i = 0; i < sizeof(uint32_t); i++) 
    {
        len_ptr[i] = sock->rx_buffer[sock->rx_tail];
        sock->rx_tail = (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
    }

    /* Validação crítica de sanidade contra corrupção por estouro */
    if (frame_len == 0 || frame_len > SOCKET_BUFFER_SIZE) 
    {
        spinlock_release(&sock->lock);
        return -2; /* Corrupção de alinhamento de buffer */
    }

    /* Passo B: Transfere os bytes puros do frame completo (incluindo MAC headers) para o Ring 3 */
    uint32_t limit = (frame_len < len) ? frame_len : len;
    while (bytes_read < limit) 
    {
        dest[bytes_read] = sock->rx_buffer[sock->rx_tail];
        sock->rx_tail = (sock->rx_tail + 1) % SOCKET_BUFFER_SIZE;
        bytes_read++;
    }

    /* Caso o buffer do aplicativo do utilizador seja curto, limpa o excesso para alinhar a cauda */
    if (bytes_read < frame_len) 
    {
        sock->rx_tail = (sock->rx_tail + (frame_len - bytes_read)) % SOCKET_BUFFER_SIZE;
    }

    /* Populamos opcionalmente a origem indicando a família que processou o pacote (AF_PACKET) */
    if (bytes_read > 0 && src_addr && addrlen && *addrlen >= sizeof(struct sockaddr_ll)) 
    {
        struct sockaddr_ll* src_sll = (struct sockaddr_ll*)src_addr;
        memset(src_sll, 0, sizeof(struct sockaddr_ll));
        src_sll->sll_family = AF_PACKET; /* 17 */
        *addrlen = sizeof(struct sockaddr_ll);
    }

    spinlock_release(&sock->lock);
    return (long)bytes_read; /* Devolve a quantidade de bytes do frame lidos com sucesso */
}

/**
 * @brief packet_input - Função de injeção atómica chamada pelo driver físico da placa (Sniffer Hook).
 *                       Deve ser invocada no início do processamento de recepção do driver de rede.
 * 
 * @param global_socket_list Ponteiro para a lista de sockets a varrer (pode passar g_bound_sockets_head).
 * @param frame              Ponteiro para os bytes brutos do frame completo capturado.
 * @param frame_len          Tamanho completo do frame Ethernet recebido em bytes.
 */
void packet_input(socket_t* global_socket_list, const void* frame, uint32_t frame_len)
{
    if (!global_socket_list || !frame || frame_len == 0 || frame_len > 1514) return;

    socket_t* curr = global_socket_list;

    /* Varre todas as sessões para identificar quem abriu um Raw Socket (PF_PACKET) */
    while (curr != NULL)
    {
        if (curr->family == PF_PACKET && curr->rx_buffer != NULL)
        {
            spinlock_acquire(&curr->lock);

            /* Calcula o espaço total exigido (4 bytes do tamanho + tamanho do frame) */
            uint32_t total_required = sizeof(uint32_t) + frame_len;
            uint32_t free_space = (curr->rx_tail - curr->rx_head - 1 + SOCKET_BUFFER_SIZE) % SOCKET_BUFFER_SIZE;

            if (free_space >= total_required)
            {
                /* Passo A: Grava o metadado do comprimento do frame (4 bytes) */
                uint8_t* len_ptr = (uint8_t*)&frame_len;
                for (uint32_t i = 0; i < sizeof(uint32_t); i++) {
                    curr->rx_buffer[curr->rx_head] = len_ptr[i];
                    curr->rx_head = (curr->rx_head + 1) % SOCKET_BUFFER_SIZE;
                }

                /* Passo B: Copia os bytes físicos completos do frame para o Ring Buffer */
                const uint8_t* raw_frame_ptr = (const uint8_t*)frame;
                for (uint32_t i = 0; i < frame_len; i++) {
                    curr->rx_buffer[curr->rx_head] = raw_frame_ptr[i];
                    curr->rx_head = (curr->rx_head + 1) % SOCKET_BUFFER_SIZE;
                }
            }
            
            spinlock_release(&curr->lock);
        }
        curr = curr->next;
    }
}

protocol_operations_t g_pf_packet_ops = {
    .bind     = pf_packet_bind,
    .connect  = pf_packet_connect,
    .sendto   = pf_packet_sendto,
    .recvfrom = pf_packet_recvfrom,
    .listen   = pf_packet_listen,
    .accept   = pf_packet_accept
};
