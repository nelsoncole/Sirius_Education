/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ethernet.c
 *    Description: Camada de Ligação de Dados (L2) - Protocolo Ethernet.
 *                 Gere o encapsulamento físico, injeção de endereços MAC
 *                 e interface direta com o driver de rede (Data-Link Layer).
 * 
 *         Author: Nelson Cole
 *   Created Date: 21/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 21/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/net/net.h>
#include <kernel/lib/string.h>
#include <kernel/klib.h>

/**
 * @brief ethernet_output - Camada de Ligação de Dados (L2).
 *                          Injeta o endereço MAC de destino resolvido pelo ARP 
 *                          no frame retido e despacha-o para o driver físico.
 * 
 * @param packet_data       Ponteiro bruto para o início do frame Ethernet guardado.
 * @param packet_len        Tamanho total do frame (incluindo o cabeçalho Ethernet).
 * @param hardware_mac      Endereço MAC físico de destino resolvido (6 bytes).
 * @return 0 em caso de sucesso absoluto, ou valor negativo em caso de falha do driver.
 */
int ethernet_output(void* packet_data, uint32_t packet_len, const uint8_t* hardware_mac)
{
    /* 1. Validação defensiva básica de ponteiros no Ring 0 */
    if (!packet_data || !hardware_mac || packet_len < sizeof(ethernet_header_t)) 
    {
        return -1;
    }

    /* 2. Mapeia a estrutura Ethernet diretamente sobre o topo do buffer recuperado */
    ethernet_header_t* eth = (ethernet_header_t*)packet_data;

    /* 
     * 3. INJEÇÃO TARDIA DO MAC (Late Binding):
     * O espaço já existia, mas estava vazio ou desatualizado. Copiamos agora o MAC 
     * físico real recebido através do ARP Reply para o campo de destino do cabeçalho.
     */
    memcpy(eth->dest_mac, hardware_mac, 6);

    /*
     * 4. INTERFACE COM O HARDWARE FÍSICO:
     * Despacha o frame finalizado diretamente para o Driver da e1000.
     * Como o pacote já foi retirado da fila do ARP, o 'arp_flush_pending_packets'
     * encarregar-se-á de libertar a memória (kfree) imediatamente após esta chamada.
     */
    int res = net_driver_transmit(packet_data, packet_len);

    if (res < 0)
    {
        kprintf("[Ethernet Error] Falha ao transmitir frame retido via driver de hardware.\n");
    }

    return res;
}
