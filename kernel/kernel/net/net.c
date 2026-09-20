/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: net.c
 *    Description: Configurações globais do subsistema de rede e propriedades
 *                 das interfaces do sistema (IP, Máscara e Gateway).
 *                 Suporte a múltiplos adaptadores físicos e registo de MACs.
 *                 Barramento Polimórfico de Entrada/Saída Full-Duplex (HAL).
 * 
 *         Author: Nelson Cole
 *   Created Date: 18/09/2026
 * 
 *    Modified By: Nelson Cole / AI Collaborator
 *  Modified Date: 20/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/net.h>
#include <kernel/kernel/net/socket.h>
#include <kernel/lib/stddef.h>
#include <kernel/klib.h>
#include <kernel/kernel/core/spinlock.h>
#include <kernel/arch/x86_64/kapi/timer.h>

#define NET_RX_QUEUE_SIZE 64
#define MAX_NETWORK_INTERFACES 4

typedef struct 
{
    uint8_t* data;
    uint32_t len;
} net_rx_packet_t;

/* Estrutura interna para controlo de cada interface viva no barramento */
typedef struct 
{
    uint32_t id;
    char     name[16]; /* Nome amigável da interface (ex: "eth0") */
    uint8_t  mac[6];
    uint32_t ip; 
    uint32_t mask;
    uint32_t gateway;
    void*    ops_table;
    bool     active;
    bool     init;
    bool     is_online;
} net_interface_t;


static net_rx_packet_t g_net_rx_queue[NET_RX_QUEUE_SIZE];
static uint32_t g_net_rx_head = 0;
static uint32_t g_net_rx_tail = 0;
static spinlock_t g_net_rx_lock = { SPINLOCK_RELEASED };

/* Tabela centralizada de múltiplas interfaces de hardware */
static net_interface_t g_net_interfaces[MAX_NETWORK_INTERFACES];
static uint32_t g_interface_count = 0;

/* Declarações dos stubs de triagem que estabilizámos nas camadas superiores */
extern int  ip_input(const void* packet_data, uint32_t packet_len);
extern int  arp_input(const void* packet_data, uint32_t packet_len);
extern void packet_input(socket_t* global_socket_list, const void* frame, uint32_t frame_len);
extern socket_t* g_bound_sockets_head;
uint64_t g_current_packet_tsc_start;

/* ESTRUTURA POLIMÓRFICA DO DRIVER DE REDE (Interface HAL Expandida) */
typedef struct 
{
    int (*transmit)(const void* buffer, uint32_t length);
    int (*receive)(const void* buffer, uint32_t length); 
    int (*open)(void);
    int (*stop)(void);
} net_device_ops_t;

/* Ponteiro global para manter retrocompatibilidade com o driver primário ativo */
static net_device_ops_t* g_active_net_driver_ops = NULL;

/**
 * @brief net_driver_register - Permite que os drivers dinâmicos (.ko) registem
 *                           os seus endereços MAC físicos e tabelas de operações
 *                           na lista global multi-placa do núcleo.
 * 
 * @param mac_addr  Ponteiro para o array de 6 bytes contendo o MAC lido do hardware.
 * @param ops_table Ponteiro para a tabela de operações polimórficas do driver.
 * @return ID único da interface atribuído no sistema (0 a 3), ou -1 em falha.
 */
int net_driver_register(const uint8_t *mac_addr, void *ops_table)
{
    if (!mac_addr || !ops_table)
    {
        kprintf("[net]: Erro: Parametros invalidos no registo de MAC.\n");
        return -1;
    }

    if (g_interface_count >= MAX_NETWORK_INTERFACES)
    {
        kprintf("[net]: Erro: Limite maximo de interfaces de rede atingido (%d).\n", MAX_NETWORK_INTERFACES);
        return -1;
    }

    uint32_t current_idx = g_interface_count;

    /* Vincula os metadados no slot disponível da lista */
    g_net_interfaces[current_idx].id = current_idx;
    g_net_interfaces[current_idx].ops_table = ops_table;
    g_net_interfaces[current_idx].active = true;
    memcpy(g_net_interfaces[current_idx].mac, mac_addr, 6);

    g_net_interfaces[current_idx].ip = 0; 
    g_net_interfaces[current_idx].mask = 0;
    g_net_interfaces[current_idx].gateway = 0;

    g_net_interfaces[current_idx].init = 0;
    g_net_interfaces[current_idx].is_online = 0;

    /* Formata e guarda o nome do dispositivo dinamicamente (ex: eth0, eth1...) */
    ksprintf(g_net_interfaces[current_idx].name, "eth%d", current_idx); 

    /* Acopla o driver primário automaticamente para manter o barramento polimórfico */
    if (g_interface_count == 0)
    {
        g_active_net_driver_ops = (net_device_ops_t*)ops_table;
    }

    g_interface_count++;

    kprintf("[net]: Interface '%s' registada com o MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
            g_net_interfaces[current_idx].name, mac_addr[0], mac_addr[1], 
            mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);


    return (int)current_idx;
}

/**
 * @brief net_get_interface_mac - Permite que subsistemas internos (como o DHCP e ARP)
 *                                 consultem com seguranca o MAC de uma interface registada.
 * 
 * @param interface_id ID numerico da placa (0 para eth0, 1 para eth1, etc).
 * @param mac_out      Ponteiro de destino para onde os 6 bytes do MAC serao copiados.
 * @return 0 em caso de sucesso, ou valor negativo em falha.
 */
int net_get_interface_mac(uint32_t interface_id, uint8_t* mac_out)
{
    if (interface_id >= MAX_NETWORK_INTERFACES || !mac_out)
    {
        return -1;
    }

    /* Valida se a placa de rede ja subiu e foi acoplada pelo modulo .ko */
    if (!g_net_interfaces[interface_id].active)
    {
        return -2; 
    }

    /* Copia os 6 bytes salvaguardados de forma simetrica */
    memcpy(mac_out, g_net_interfaces[interface_id].mac, 6);
    
    return 0;
}

int net_set_interface_ip(uint32_t ip)
{
    uint32_t interface_id = 0; 

    if (interface_id >= MAX_NETWORK_INTERFACES)
    {
        return -1;
    }

    /* Valida se a placa de rede ja subiu e foi acoplada pelo modulo .ko */
    if (!g_net_interfaces[interface_id].active)
    {
        return -2; 
    }
    
    g_net_interfaces[interface_id].ip = ip;

    return 0;
}

int net_get_interface_ip(uint32_t interface_id, uint32_t* ip_out)
{
    if (interface_id >= MAX_NETWORK_INTERFACES)
    {
        return -1;
    }

    /* Valida se a placa de rede ja subiu e foi acoplada pelo modulo .ko */
    if (!g_net_interfaces[interface_id].active)
    {
        return -2; 
    }

    *ip_out = g_net_interfaces[interface_id].ip;
    
    return 0;
}

int net_set_interface_mask(uint32_t mask)
{
    uint32_t interface_id = 0;

    if (interface_id >= MAX_NETWORK_INTERFACES)
    {
        return -1;
    }

    /* Valida se a placa de rede ja subiu e foi acoplada pelo modulo .ko */
    if (!g_net_interfaces[interface_id].active)
    {
        return -2; 
    }
    
    g_net_interfaces[interface_id].mask = mask;
    
    return 0;
}

int net_get_interface_mask(uint32_t interface_id, uint32_t* mask_out)
{
    if (interface_id >= MAX_NETWORK_INTERFACES)
    {
        return -1;
    }

    /* Valida se a placa de rede ja subiu e foi acoplada pelo modulo .ko */
    if (!g_net_interfaces[interface_id].active)
    {
        return -2; 
    }

    *mask_out = g_net_interfaces[interface_id].mask;
    
    return 0;
}

int net_set_interface_gateway(uint32_t gateway)
{
    uint32_t interface_id = 0;

    if (interface_id >= MAX_NETWORK_INTERFACES)
    {
        return -1;
    }

    /* Valida se a placa de rede ja subiu e foi acoplada pelo modulo .ko */
    if (!g_net_interfaces[interface_id].active)
    {
        return -2; 
    }
    
    g_net_interfaces[interface_id].gateway = gateway;
    
    return 0;
}

int net_get_interface_gateway(uint32_t interface_id, uint32_t* gateway_out)
{
    if (interface_id >= MAX_NETWORK_INTERFACES)
    {
        return -1;
    }

    /* Valida se a placa de rede ja subiu e foi acoplada pelo modulo .ko */
    if (!g_net_interfaces[interface_id].active)
    {
        return -2; 
    }

    *gateway_out = g_net_interfaces[interface_id].gateway;
    
    return 0;
}

int net_get_interface_init(uint32_t interface_id, bool flag)
{
    if (interface_id >= MAX_NETWORK_INTERFACES)
    {
        return -1;
    }

    /* Valida se a placa de rede ja subiu e foi acoplada pelo modulo .ko */
    if (!g_net_interfaces[interface_id].active)
    {
        return -2; 
    }

    g_net_interfaces[interface_id].init = flag;
    
    return 0;
}

int net_get_interface_is_online(uint32_t interface_id, bool flag)
{
    if (interface_id >= MAX_NETWORK_INTERFACES)
    {
        return -1;
    }

    /* Valida se a placa de rede ja subiu e foi acoplada pelo modulo .ko */
    if (!g_net_interfaces[interface_id].active)
    {
        return -2; 
    }

    g_net_interfaces[interface_id].is_online = flag;
    
    return 0;
}

int net_driver_transmit(const void* buffer, uint32_t packet_size)
{
    if (!buffer || packet_size == 0) 
    {
        return -1;
    }

    if (!g_active_net_driver_ops || !g_active_net_driver_ops->transmit)
    {
        return -2; 
    }

    return g_active_net_driver_ops->transmit(buffer, packet_size);
}

int net_driver_receive(const void* buffer, uint32_t packet_size)
{
    if (!buffer || packet_size == 0 || packet_size > 1514) 
    {
        return -1;
    }

    spinlock_acquire(&g_net_rx_lock);

    uint32_t next_head = (g_net_rx_head + 1) % NET_RX_QUEUE_SIZE;
    if (next_head == g_net_rx_tail) 
    {
        spinlock_release(&g_net_rx_lock);
        return -2; 
    }

    uint8_t* packet_copy = (uint8_t*)kmalloc(packet_size);
    if (!packet_copy) 
    {
        spinlock_release(&g_net_rx_lock);
        return -3;
    }
    memcpy(packet_copy, buffer, packet_size);

    g_net_rx_queue[g_net_rx_head].data = packet_copy;
    g_net_rx_queue[g_net_rx_head].len  = packet_size;
    g_net_rx_head = next_head;

    spinlock_release(&g_net_rx_lock);
    return 0; 
}

extern int net_init_dhcp_client(void);
void network_rx_thread(void)
{
    kprintf("[NET CORE] Thread de processamento de pacotes (RX) ativa em Ring 0.\n");

    net_init_dhcp_client();

    while (1)
    {
        uint8_t* packet_data = NULL;
        uint32_t packet_len  = 0;
        unsigned long flags;

        /* CRÍTICO FIX: Desativa as interrupções antes de prender o Lock em contexto de Thread!
           Isto impede que a IRQ da placa de rede interrompa esta linha e cause o Deadlock. */
        flags = spinlock_lock_irqsave(&g_net_rx_lock);
        
        if (g_net_rx_head != g_net_rx_tail)
        {
            packet_data = g_net_rx_queue[g_net_rx_tail].data;
            packet_len  = g_net_rx_queue[g_net_rx_tail].len;
            g_net_rx_tail = (g_net_rx_tail + 1) % NET_RX_QUEUE_SIZE;
        }
        
        /* Restaura as interrupções imediatamente após ler da fila circular */
        spinlock_unlock_irqrestore(&g_net_rx_lock, flags);

        if (!packet_data)
        {
            __asm__ __volatile__("pause");
            continue;
        }

        /* Guarda o momento exato em que o pacote começou a ser tratado */
        uint64_t tsc_start = read_tsc();
        /* Chame as funções de input passando este valor se necessário, 
           ou guarde-o numa variável global/estrutura temporária acessível pelo icmp_input */
        g_current_packet_tsc_start = tsc_start;

        packet_input(g_bound_sockets_head, packet_data, packet_len);

        ethernet_header_t* eth = (ethernet_header_t*)packet_data;
        uint16_t ethertype = ntohs(eth->ethertype);

        const void* data = packet_data + sizeof(ethernet_header_t); 
        uint32_t len = packet_len - sizeof(ethernet_header_t);

        if (ethertype == ETH_P_IP) 
        {
            ip_input(data, len);
        } 
        else if (ethertype == ETH_P_ARP) 
        {
            arp_input(data, len);
        }

        kfree(packet_data);
    }
}

extern void arp_init(void);
void net_init(void)
{
    g_active_net_driver_ops = NULL;
    g_interface_count = 0;
    memset(g_net_interfaces, 0, sizeof(g_net_interfaces));

    arp_init();
}