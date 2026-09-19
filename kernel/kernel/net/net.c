/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: net.c
 *    Description: Configurações globais do subsistema de rede e propriedades
 *                 da interface padrão do sistema (IP, Máscara e Gateway).
 *                 Barramento Polimórfico de Entrada/Saída Full-Duplex (HAL).
 * 
 *         Author: Nelson Cole
 *   Created Date: 18/09/2026
 * 
 *    Modified By: Nelson Cole / AI Collaborator
 *  Modified Date: 18/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/net.h>
#include <kernel/kernel/net/socket.h>
#include <kernel/lib/stddef.h>
#include <kernel/klib.h>

/* Fila Circular Global de Pacotes Recebidos e Pendentes para Processamento */
#define NET_RX_QUEUE_SIZE 64

typedef struct {
    uint8_t* data;
    uint32_t len;
} net_rx_packet_t;

static net_rx_packet_t g_net_rx_queue[NET_RX_QUEUE_SIZE];
static uint32_t g_net_rx_head = 0;
static uint32_t g_net_rx_tail = 0;
static spinlock_t g_net_rx_lock = { SPINLOCK_RELEASED };

/* Declarações dos stubs de triagem que estabilizámos nas camadas superiores */
extern int  ip_input(const void* packet_data, uint32_t packet_len);
extern int  arp_input(const void* packet_data, uint32_t packet_len);
extern void packet_input(socket_t* global_socket_list, const void* frame, uint32_t frame_len);
extern socket_t* g_bound_sockets_head; /* Lista global de sockets para o Sniffer Hook */

/* 
 * DEFINIÇÃO DAS GLOBAIS PROVISÓRIAS (Em Network Byte Order / Big-Endian)
 * IP Padrão: 10.0.2.15 (0x0A00020F) -> Padrão NAT do QEMU
 * Máscara:   255.255.255.0 (0xFFFFFF00)
 * Gateway:   10.0.2.2 (0x0A000202) -> Roteador virtual do QEMU
 */
uint32_t g_net_interface_ip      = 0x0F02000A; /* Armazenado pronto para rede (htons/htonl) */
uint32_t g_net_interface_mask    = 0x00FFFFFF;
uint32_t g_net_interface_gateway = 0x0202000A;

/*
 * ENDEREÇO MAC FÍSICO GLOBAL DA INTERFACE:
 * Centralizado aqui para alimentar simetricamente o ARP, o DHCP e os drivers PCIe.
 */
uint8_t g_net_interface_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };


/* 
 * ESTRUTURA POLIMÓRFICA DO DRIVER DE REDE (Interface HAL Expandida)
 * Define as assinaturas que qualquer placa de rede física deve implementar.
 */
typedef struct {
    int (*transmit)(const void* buffer, uint32_t length);
    int (*receive)(const void* buffer, uint32_t length); /* ADICIONADO: Gancho de receção polimórfica */
    int (*open)(void);
    int (*stop)(void);
} net_device_ops_t;

/* Ponteiro global do driver ativo registado no sistema */
static net_device_ops_t* g_active_net_driver_ops = NULL;

/**
 * @brief net_driver_register - Permite que qualquer interface de rede física 
 *                              (Ethernet PCIe, Wi-Fi, ou Modem USB) se acoble 
 *                              ao barramento polimórfico do Kernel.
 * 
 * @param ops_table Ponteiro para a estrutura de operações do hardware (net_device_ops_t).
 */
void net_driver_register(void* ops_table)
{
    if (!ops_table) return;
    
    /* 
     * Vinculação Dinâmica Genérica: O Kernel passa a apontar para o novo hardware,
     * seja ele uma placa Ethernet (e1000), um chip Wi-Fi, ou um dongle USB Modem.
     */
    g_active_net_driver_ops = (net_device_ops_t*)ops_table;
    
    kprintf("[NET CORE] Interface de rede (Ethernet/Wi-Fi/USB) acoplada ao barramento polimorfico.\n");
}

/**
 * @brief net_driver_transmit - Função central de saída de pacotes do Kernel.
 *                              Invoca o método real do driver PCIe de forma polimórfica.
 * 
 * @param buffer      Ponteiro para os bytes contíguos alocados no Heap (IP/ARP/Packet).
 * @param packet_size Tamanho total do pacote a ser transmitido.
 * @return 0 em caso de sucesso absoluto, ou valor negativo em falha.
 */
int net_driver_transmit(const void* buffer, uint32_t packet_size)
{
    if (!buffer || packet_size == 0) return -1;

    /* 1. Verifica de forma segura se existe um driver de hardware ativo no sistema */
    if (!g_active_net_driver_ops || !g_active_net_driver_ops->transmit)
    {
        /* 
         * Salvaguarda silenciosa: Se o cabo estiver desligado ou o driver não inicializou,
         * descartamos o pacote para não gerar um crash de ponteiro nulo em Ring 0.
         */
        return -2; 
    }

    /* 2. O MILAGRE DO POLIMORFISMO: Invoca a função real de escrita física da placa ativa */
    return g_active_net_driver_ops->transmit(buffer, packet_size);
}

/**
 * @brief net_driver_receive - CHAMADO DIRETO DA ISR DE QUALQUER PLACA (Top-Half).
 *                             Apenas aloca, copia o frame bruto e sai da interrupção em nanosegundos.
 */
int net_driver_receive(const void* buffer, uint32_t packet_size)
{
    if (!buffer || packet_size == 0 || packet_size > 1514) return -1;

    spinlock_acquire(&g_net_rx_lock);

    /* Verifica se a fila de subida do Kernel está cheia */
    uint32_t next_head = (g_net_rx_head + 1) % NET_RX_QUEUE_SIZE;
    if (next_head == g_net_rx_tail) 
    {
        /* Buffer do Kernel saturado, dropa o pacote fisicamente para proteger o Ring 0 */
        spinlock_release(&g_net_rx_lock);
        return -2; 
    }

    /* Aloca um bloco contíguo estável no Heap para carregar o pacote fora da ISR */
    uint8_t* packet_copy = (uint8_t*)kmalloc(packet_size);
    if (!packet_copy) {
        spinlock_release(&g_net_rx_lock);
        return -3;
    }
    memcpy(packet_copy, buffer, packet_size);

    /* Enfileira o pacote na RX Queue */
    g_net_rx_queue[g_net_rx_head].data = packet_copy;
    g_net_rx_queue[g_net_rx_head].len  = packet_size;
    g_net_rx_head = next_head;

    spinlock_release(&g_net_rx_lock);

    /* 
     * Se tiver um mecanismo de Wakeup de Threads no seu Scheduler,
     * acordaria aqui a thread 'network_rx_thread'.
     */
    return 0; 
}

/**
 * @brief network_rx_thread - Thread de Kernel Dedicada (Bottom-Half).
 *                            Roda em Ring 0 com interrupções ligadas, livre de deadlocks.
 */
void network_rx_thread(void)
{
    kprintf("[NET CORE] Thread de processamento de pacotes (RX) ativa em Ring 0.\n");

    while (1)
    {
        uint8_t* packet_data = NULL;
        uint32_t packet_len  = 0;

        /* 1. Retira o pacote da fila de forma atómica e rápida */
        spinlock_acquire(&g_net_rx_lock);
        if (g_net_rx_head != g_net_rx_tail)
        {
            packet_data = g_net_rx_queue[g_net_rx_tail].data;
            packet_len  = g_net_rx_queue[g_net_rx_tail].len;
            g_net_rx_tail = (g_net_rx_tail + 1) % NET_RX_QUEUE_SIZE;
        }
        spinlock_release(&g_net_rx_lock);

        /* 2. Se a fila estava vazia, cede o CPU para não fritar o núcleo */
        if (!packet_data)
        {
            __asm__ __volatile__("pause");
            /* Se o seu scheduler tiver yield: scheduler_yield(); */
            continue;
        }

        /* 3. PROCESSAMENTO SEGURO FORA DE INTERRUPÇÃO */
        /* Hook do Sniffer (PF_PACKET) */
        packet_input(g_bound_sockets_head, packet_data, packet_len);

        /* Triagem canônica Ethernet por EtherType */
        ethernet_header_t* eth = (ethernet_header_t*)packet_data;
        uint16_t ethertype = ntohs(eth->ethertype);

        if (ethertype == ETH_P_IP) {
            ip_input(packet_data, packet_len);
        } 
        else if (ethertype == ETH_P_ARP) {
            arp_input(packet_data, packet_len);
        }

        /* 4. LIBERTAÇÃO OBRIGATÓRIA: Limpa a cópia do Heap após o consumo completo pelas camadas */
        kfree(packet_data);
    }
}

void net_init(void)
{
    g_active_net_driver_ops = NULL;
}