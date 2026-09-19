/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: arp.c
 *    Description: Subsistema ARP (Address Resolution Protocol).
 *                 Gere a ARP Cache na RAM, processa pacotes de entrada
 *                 e forja requisições de mapeamento IP/MAC via hardware.
 * 
 *         Author: Nelson Cole
 *   Created Date: 18/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 18/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/net.h>
#include <kernel/kernel/net/socket.h>
#include <kernel/kernel/core/spinlock.h>
#include <kernel/klib.h>
#include <kernel/lib/string.h>

#define ARP_CACHE_MAX 32
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

/* Estrutura de uma entrada dinâmica na Tabela ARP Cache */
typedef struct {
    uint32_t ip_addr;     /* Chave de Busca (Big-Endian) */
    uint8_t  mac_addr[6]; /* Resposta Física */
    uint8_t  is_valid;    /* Flag de controle de sessão */
} arp_entry_t;

/* Tabela ARP e o seu trinco de isolamento SMP */
static arp_entry_t g_arp_cache[ARP_CACHE_MAX];
static spinlock_t  g_arp_lock = { SPINLOCK_RELEASED };

/* MAC Provisório de Hardware do Sirius OS (Substitua pelo MAC lido da sua e1000/rtl8139) */
static const uint8_t g_my_hardware_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };

/**
 * @brief Inicializa e limpa a tabela ARP Cache no boot.
 */
void arp_init(void) 
{
    spin_lock_init(&g_arp_lock);
    memset(g_arp_cache, 0, sizeof(g_arp_cache));
    kprintf("[ARP] Tabela ARP Cache inicializada com sucesso na RAM.\n");
}

/**
 * @brief Procura o MAC associado a um IP dentro da Cache local.
 * @return Ponteiro para os 6 bytes do MAC, ou NULL se não encontrado.
 */
uint8_t* arp_cache_lookup(uint32_t ip_addr) 
{
    spinlock_acquire(&g_arp_lock);
    for (int i = 0; i < ARP_CACHE_MAX; i++) 
    {
        if (g_arp_cache[i].is_valid && g_arp_cache[i].ip_addr == ip_addr) 
        {
            spinlock_release(&g_arp_lock);
            return g_arp_cache[i].mac_addr;
        }
    }
    spinlock_release(&g_arp_lock);
    return NULL;
}

/**
 * @brief Insere ou atualiza um par IP/MAC de forma atómica na Cache.
 */
void arp_cache_insert(uint32_t ip_addr, const uint8_t* mac_addr) 
{
    spinlock_acquire(&g_arp_lock);
    
    /* 1. Procura se já existe para atualizar */
    for (int i = 0; i < ARP_CACHE_MAX; i++) {
        if (g_arp_cache[i].is_valid && g_arp_cache[i].ip_addr == ip_addr) {
            memcpy(g_arp_cache[i].mac_addr, mac_addr, 6);
            spinlock_release(&g_arp_lock);
            return;
        }
    }

    /* 2. Se não existe, insere num slot livre */
    for (int i = 0; i < ARP_CACHE_MAX; i++) {
        if (!g_arp_cache[i].is_valid) {
            g_arp_cache[i].ip_addr = ip_addr;
            memcpy(g_arp_cache[i].mac_addr, mac_addr, 6);
            g_arp_cache[i].is_valid = 1;
            spinlock_release(&g_arp_lock);
            return;
        }
    }
    spinlock_release(&g_arp_lock);
}

/**
 * @brief arp_request - Forja e transmite um frame ARP Request (Broadcast) na rede.
 */
int arp_request(uint32_t target_ip) 
{
    /* Tamanho total: 14 bytes do Header Ethernet + 28 bytes do Header ARP = 42 bytes */
    uint32_t packet_size = sizeof(ethernet_header_t) + sizeof(arp_header_t);
    
    uint8_t* buffer = (uint8_t*)kmalloc(packet_size);
    if (!buffer) return -1;
    memset(buffer, 0, packet_size);

    /* 1. CONFIGURAÇÃO DO CADEÇALHO ETHERNET (Camada 2) */
    ethernet_header_t* eth = (ethernet_header_t*)buffer;
    /* Broadcast MAC: FF:FF:FF:FF:FF:FF (Para que todas as placas leiam o frame) */
    memset(eth->dest_mac, 0xFF, 6);
    memcpy(eth->src_mac, g_my_hardware_mac, 6);
    eth->ethertype = htons(ETH_P_ARP); /* Sinaliza payload como pacote ARP (0x0806) */

    /* 2. CONFIGURAÇÃO DO CABEÇALHO ARP (Camada 2.5) */
    arp_header_t* arp = (arp_header_t*)(buffer + sizeof(ethernet_header_t));
    arp->hw_type   = htons(1);         /* 1 = Ethernet */
    arp->proto_type = htons(ETH_P_IP);  /* 0x0800 = IPv4 */
    arp->hw_len    = 6;
    arp->proto_len   = 4;
    arp->opcode    = htons(ARP_OP_REQUEST); /* Operação REQUEST */
    
    memcpy(arp->src_mac, g_my_hardware_mac, 6);
    arp->src_ip    = g_net_interface_ip; /* Nosso IP lido dinamicamente do net.c */
    
    /* dest_mac fica a zero no Request pois estamos justamente a tentar descobri-lo */
    memset(arp->dest_mac, 0x00, 6);
    arp->dest_ip   = target_ip;

    kprintf("[ARP] Transmitindo Request: Quem tem o IP 0x%x? Diga a %02x:%02x...\n", 
            ntohl(target_ip), g_my_hardware_mac[0], g_my_hardware_mac[1]);

    /* 
    INJEÇÃO FÍSICA NO CABO FÍSICO: */
    int res = net_driver_transmit(buffer, packet_size);

    kfree(buffer);
    return res;
}

/**
 * @brief arp_input - Processa pacotes ARP entrantes do cabo (Gatilho ativado pelo hardware).
 */
int arp_input(const void* packet_data, uint32_t packet_len) 
{
    int res = -1;

    if (!packet_data || packet_len < (sizeof(ethernet_header_t) + sizeof(arp_header_t))) {
        return -1;
    }

    /* Salta os 14 bytes do cabeçalho Ethernet para ler a estrutura ARP */
    arp_header_t* arp = (arp_header_t*)((const uint8_t*)packet_data + sizeof(ethernet_header_t));

    /* Validações estruturais básicas de protocolo */
    if (ntohs(arp->hw_type) != 1 || ntohs(arp->proto_type) != ETH_P_IP) return -2;

    /* REGISTO DE APRENDIZADO (ARP Learning): Salva o IP/MAC de quem enviou na nossa Cache */
    arp_cache_insert(arp->src_ip, arp->src_mac);

    uint16_t opcode = ntohs(arp->opcode);

    if (opcode == ARP_OP_REQUEST) 
    {
        /* CASO A: Alguém na rede perguntou pelo NOSSO IP. Precisamos de responder (REPLY) */
        if (arp->dest_ip == g_net_interface_ip) 
        {
            kprintf("[ARP Input] Request recebido! Respondendo autonomamente...\n");

            uint32_t reply_size = sizeof(ethernet_header_t) + sizeof(arp_header_t);
            uint8_t* reply_buf = (uint8_t*)kmalloc(reply_size);
            if (!reply_buf) return -3;

            /* Configura Ethernet de Retorno (Unicast Direto para quem perguntou) */
            ethernet_header_t* reply_eth = (ethernet_header_t*)reply_buf;
            memcpy(reply_eth->dest_mac, arp->src_mac, 6);
            memcpy(reply_eth->src_mac, g_my_hardware_mac, 6);
            reply_eth->ethertype = htons(ETH_P_ARP);

            /* Configura ARP Reply */
            arp_header_t* reply_arp = (arp_header_t*)(reply_buf + sizeof(ethernet_header_t));
            reply_arp->hw_type   = htons(1);
            reply_arp->proto_type = htons(ETH_P_IP);
            reply_arp->hw_len    = 6;
            reply_arp->proto_len   = 4;
            reply_arp->opcode    = htons(ARP_OP_REPLY); /* Resposta */

            memcpy(reply_arp->src_mac, g_my_hardware_mac, 6);
            reply_arp->src_ip    = g_net_interface_ip;
            
            /* Destinatário agora é quem originou o pedido */
            memcpy(reply_arp->dest_mac, arp->src_mac, 6);
            reply_arp->dest_ip   = arp->src_ip;

            /* Envia o frame ARP Reply de volta para a placa de rede */
            res = net_driver_transmit(reply_buf, reply_size);
            
            kfree(reply_buf);
        }
    } 
    else if (opcode == ARP_OP_REPLY) 
    {
        /* 
         * CASO B: Uma resposta formal para um Request que o Sirius OS enviou anteriormente.
         * Exibe o endereço MAC completo de 6 bytes de forma legível e canónica (XX:XX:XX:XX:XX:XX).
         */
        kprintf("[ARP Input] Reply recebido! Mapeamento resolvido: IP %s -> MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
                inet_ntoa(arp->src_ip),
                arp->src_mac[0], arp->src_mac[1], arp->src_mac[2],
                arp->src_mac[3], arp->src_mac[4], arp->src_mac[5]);
        
        /* 
         * ====================================================================
         * MECANISMO DE SINALIZAÇÃO ATIVA (WAKEUP) PARA MULTICORE / SMP
         * ====================================================================
         * Como a Cache local já foi atualizada de forma atómica na entrada da função 
         * via 'arp_cache_insert', o mapeamento IP->MAC já está disponível na RAM.
         * 
         * Se o teu agendador (Scheduler) implementar listas de bloqueio para Threads 
         * suspensas à espera de I/O, deves disparar o Wakeup das threads que aguardavam 
         * por este 'arp->src_ip' específico, acordando-as do loop passivo 'hlt/pause' 
         * de dentro do 'ip_output' para que possam ejetar os frames no cabo imediatamente!
         */
         
        // Exemplo de integração futura com o teu Scheduler:
        // scheduler_wakeup_threads_waiting_for_ip(arp->src_ip);
    }

    return res;
}