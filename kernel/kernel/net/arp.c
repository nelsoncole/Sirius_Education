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
    /* FILA DE ESPERA ARP */
    void*    pending_packet; /* Ponteiro para o tx_buffer guardado */
    uint32_t pending_len;    /* Tamanho do frame guardado */
} arp_entry_t;

/* Tabela ARP e o seu trinco de isolamento SMP */
static arp_entry_t g_arp_cache[ARP_CACHE_MAX];
static spinlock_t  g_arp_lock = { SPINLOCK_RELEASED };

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
 * @return 0 para MAC encontrada, ou -1 se não encontrado.
 */

int arp_cache_lookup(uint32_t ip_addr, uint8_t *mac_addr)
{
    if (!mac_addr)
        return -1;

    spinlock_acquire(&g_arp_lock);

    for (int i = 0; i < ARP_CACHE_MAX; i++)
    {
        if (g_arp_cache[i].is_valid &&
            g_arp_cache[i].ip_addr == ip_addr)
        {
            memcpy(mac_addr, g_arp_cache[i].mac_addr, 6);

            spinlock_release(&g_arp_lock);
            return 0;
        }
    }

    spinlock_release(&g_arp_lock);
    return -1;
}

/**
 * @brief Insere ou atualiza um par IP/MAC de forma atómica na Cache.
 */
static uint32_t g_arp_victim_idx = 0; /* Índice circular para substituição */

void arp_cache_insert(uint32_t ip_addr, const uint8_t* mac_addr) 
{
    spinlock_acquire(&g_arp_lock);
    
    /* 1. Procura se já existe para atualizar (Evita duplicados) */
    for (int i = 0; i < ARP_CACHE_MAX; i++) {
        if (g_arp_cache[i].is_valid && g_arp_cache[i].ip_addr == ip_addr) {
            memcpy(g_arp_cache[i].mac_addr, mac_addr, 6);
            spinlock_release(&g_arp_lock);
            return;
        }
    }

    /* 2. Se não existe, procura por um slot livre */
    for (int i = 0; i < ARP_CACHE_MAX; i++) {
        if (!g_arp_cache[i].is_valid) {
            g_arp_cache[i].ip_addr = ip_addr;
            memcpy(g_arp_cache[i].mac_addr, mac_addr, 6);
            g_arp_cache[i].is_valid = 1;
            spinlock_release(&g_arp_lock);
            return;
        }
    }

    /* 3. BLINDAGEM CONTRA TABELA CHEIA: Sobrescreve uma entrada antiga (Vítima) 
       Isto impede que a Cache transborde ou bloqueie novas conexões */
    uint32_t victim = g_arp_victim_idx;
    
    g_arp_cache[victim].ip_addr = ip_addr;
    memcpy(g_arp_cache[victim].mac_addr, mac_addr, 6);
    g_arp_cache[victim].is_valid = 1;
    
    /* Avança o ponteiro de substituição de forma circular */
    g_arp_victim_idx = (g_arp_victim_idx + 1) % ARP_CACHE_MAX;

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
    net_get_interface_mac(0, eth->src_mac);
    eth->ethertype = htons(ETH_P_ARP); /* Sinaliza payload como pacote ARP (0x0806) */

    /* 2. CONFIGURAÇÃO DO CABEÇALHO ARP (Camada 2.5) */
    arp_header_t* arp = (arp_header_t*)(buffer + sizeof(ethernet_header_t));
    arp->hw_type   = htons(1);         /* 1 = Ethernet */
    arp->proto_type = htons(ETH_P_IP);  /* 0x0800 = IPv4 */
    arp->hw_len    = 6;
    arp->proto_len   = 4;
    arp->opcode    = htons(ARP_OP_REQUEST); /* Operação REQUEST */

    net_get_interface_mac(0, arp->src_mac);
    uint32_t my_ip = 0x10101010;
    net_get_interface_ip(0, &my_ip);
    arp->src_ip    = my_ip;
    
    /* dest_mac fica a zero no Request pois estamos justamente a tentar descobri-lo */
    memset(arp->dest_mac, 0x00, 6);
    arp->dest_ip   = target_ip;

    kprintf("[ARP] Transmitindo Request: Quem tem o IP %s? Diga a %02x:%02x...\n", 
            inet_ntoa(target_ip), eth->src_mac[0], eth->src_mac[1]);

    /* 
    INJEÇÃO FÍSICA NO CABO FÍSICO: */
    int res = net_driver_transmit(buffer, packet_size);

    kfree(buffer);
    return res;
}

/**
 * @brief arp_gratuitous - Emite um ARP Gratuito para anunciar o novo IP e detetar conflitos.
 * @param my_new_ip O endereço IP acabado de receber do DHCP (Big-Endian).
 */
int arp_gratuitous(uint32_t my_new_ip)
{
    /* Tamanho total: 14 bytes Ethernet + 28 bytes ARP = 42 bytes */
    uint32_t packet_size = sizeof(ethernet_header_t) + sizeof(arp_header_t);
    
    uint8_t* buffer = (uint8_t*)kmalloc(packet_size);
    if (!buffer) return -1;
    memset(buffer, 0, packet_size);

    /* 1. CONFIGURAÇÃO DO CABEÇALHO ETHERNET (Camada 2) */
    ethernet_header_t* eth = (ethernet_header_t*)buffer;
    /* Destino obrigatoriamente em Broadcast para atualizar as tabelas ARP de toda a rede local */
    memset(eth->dest_mac, 0xFF, 6);
    net_get_interface_mac(0, eth->src_mac);
    eth->ethertype = htons(ETH_P_ARP); /* 0x0806 */

    /* 2. CONFIGURAÇÃO DO CABEÇALHO ARP (Camada 2.5) */
    arp_header_t* arp = (arp_header_t*)(buffer + sizeof(ethernet_header_t));
    arp->hw_type    = htons(1);         /* 1 = Ethernet */
    arp->proto_type = htons(ETH_P_IP);  /* 0x0800 = IPv4 */
    arp->hw_len     = 6;
    arp->proto_len    = 4;
    arp->opcode     = htons(ARP_OP_REQUEST); /* Continua a ser uma operação de Request */

    /* Preenche a origem com os dados do seu Kernel */
    net_get_interface_mac(0, arp->src_mac);
    arp->src_ip     = my_new_ip;
    
    /* REGRA DO ARP GRATUITO: O MAC de destino fica a zeros, mas o IP de destino é o seu próprio IP */
    memset(arp->dest_mac, 0x00, 6);
    arp->dest_ip    = my_new_ip; /* <-- CRÍTICO: dest_ip IGUAL ao src_ip */

    kprintf("[ARP] A emitir ARP Gratuito para anunciar o IP %s a rede...\n", inet_ntoa(my_new_ip));

    /* Injeção física via driver de rede */
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

    if (!packet_data || packet_len < sizeof(arp_header_t)) {
        return -1;
    }
    
    /* Ler a estrutura ARP */
    arp_header_t* arp = (arp_header_t*)packet_data;

    /* Validações estruturais básicas de protocolo */
    if (ntohs(arp->hw_type) != 1 || ntohs(arp->proto_type) != ETH_P_IP) return -2;

    if (arp->hw_len != 6 || arp->proto_len != 4) return -2;

    /* REGISTO DE APRENDIZADO (ARP Learning): Salva o IP/MAC de quem enviou na nossa Cache */
    arp_cache_insert(arp->src_ip, arp->src_mac);

    uint16_t opcode = ntohs(arp->opcode);

    if (opcode == ARP_OP_REQUEST) 
    {
        /* CASO A: Alguém na rede perguntou pelo NOSSO IP. Precisamos de responder (REPLY) */
        uint32_t my_ip = 0;
        net_get_interface_ip(0, &my_ip);
        if (arp->dest_ip == my_ip) 
        {
            kprintf("[ARP Input] Request recebido! Respondendo autonomamente...\n");

            uint32_t reply_size = sizeof(ethernet_header_t) + sizeof(arp_header_t);
            uint8_t* reply_buf = (uint8_t*)kmalloc(reply_size);
            if (!reply_buf)
            {
                return -3;
            }

            /* Configura Ethernet de Retorno (Unicast Direto para quem perguntou) */
            ethernet_header_t* reply_eth = (ethernet_header_t*)reply_buf;
            memcpy(reply_eth->dest_mac, arp->src_mac, 6);
            net_get_interface_mac(0, reply_eth->src_mac);
            reply_eth->ethertype = htons(ETH_P_ARP);

            /* Configura ARP Reply */
            arp_header_t* reply_arp = (arp_header_t*)(reply_buf + sizeof(ethernet_header_t));
            reply_arp->hw_type   = htons(1);
            reply_arp->proto_type = htons(ETH_P_IP);
            reply_arp->hw_len    = 6;
            reply_arp->proto_len   = 4;
            reply_arp->opcode    = htons(ARP_OP_REPLY); /* Resposta */

            net_get_interface_mac(0, reply_arp->src_mac);
            reply_arp->src_ip    = my_ip;
            
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