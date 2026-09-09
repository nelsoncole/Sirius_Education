/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: msi.c
 *    Description: Subsistema de Interrupções Sinalizadas por Mensagem (MSI).
 *                 Mapeia dinamicamente os vetores da IDT a partir de 81 (0x51)
 *                 e configura as mensagens de escrita de dados no barramento
 *                 do Local APIC através do espaço de configuração PCI.
 * 
 *         Author: Nelson Cole
 *   Created Date: 08/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 09/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/kapi/msi.h>
#include <kernel/drivers/bus/pci.h>
#include <kernel/arch/x86_64/cpu/cpu.h> /* Para get_current_cpu_id() */
#include <kernel/klib.h>

#define MSI_VECTOR_BASE    81

/* Pool estável de callbacks do vetor de alto nível */
msi_handler_t fnvetors_handler_msi[MAX_MSI_PINS];

/**
 * Aloca um canal MSI estável associando o hardware à pilha APIC de 64 bits.
 */
int apic_send_msi(pci_device_t *dev, void (*fuc)(void))
{
    if (!dev) return -1;

    uint8_t bus      = dev->address.bus;
    uint8_t slot     = dev->address.device;
    uint8_t function = dev->address.function;

    uint32_t cap_reg  = 0;
    uint8_t capp_addr = 0;

    int target_slot = -1;
    for (int i = 0; i < MAX_MSI_PINS; i++)
    {
        if (fnvetors_handler_msi[i] == NULL)
        {
            target_slot = i;
            break;
        }
    }

    if (target_slot == -1)
    {
        kprintf("[MSI] Erro: Pool de interrupcoes por mensagem esgotado.\n");
        return -1;
    }

    /* 1. Recupera o ponteiro inicial das Capabilities (Offset 0x34) */
    capp_addr = (uint8_t)(pci_config_read_dword(bus, slot, function, 0x34) & 0xFF);
    
    while (capp_addr != 0)
    {
        cap_reg = pci_config_read_dword(bus, slot, function, capp_addr);
        if ((cap_reg & 0xFF) == 0x05) break; /* ID 0x05 = MSI */

        capp_addr = (cap_reg >> 8) & 0xFF;
    }

    if (capp_addr == 0)
    {
        kprintf("[MSI] Dispositivo nao suporta MSI nativo.\n");
        return -1;
    }

    /* 2. Executa a desativação de INTx# tradicional (Escreve 1 no Bit 10 do registo PCI Command) */
    uint32_t pci_cmd = pci_config_read_dword(bus, slot, function, 0x04); 
    pci_cmd |= (1 << 10);
    pci_config_write_dword(bus, slot, function, 0x04, pci_cmd);

    /* 
     * CORREÇÃO 1: Lê o Message Control de forma alinhada. 
     * Ele ocupa os bits 16-31 do primeiro DWORD da capacidade msi.
     */
    uint16_t msg_control = (uint16_t)((cap_reg >> 16) & 0xFFFF);

    /* Verifica se o hardware suporta endereçamento de 64 bits (Bit 7 do Message Control) */
    int is_64bit = (msg_control & (1 << 7)) ? 1 : 0;

    /* 3. Preenche o Message Address apontando para o Local APIC da CPU alvo */
    unsigned long current_lapic_id = get_current_cpu_id();
    uint32_t msi_addr = 0xFEE00000UL | (current_lapic_id << 12);
    
    pci_config_write_dword(bus, slot, function, capp_addr + 0x04, msi_addr);

    /* CORREÇÃO 2: Configuração dinâmica de offsets dependendo do suporte a 64 bits */
    uint8_t data_offset = 0;
    if (is_64bit)
    {
        /* Se suporta 64-bits, o Address Upper fica em +0x08 e o Data vai para +0x0C */
        pci_config_write_dword(bus, slot, function, capp_addr + 0x08, 0); // Destino em low-memory
        data_offset = capp_addr + 0x0C;
    }
    else
    {
        /* Se for apenas 32-bits, o Data fica imediatamente em +0x08 */
        data_offset = capp_addr + 0x08;
    }

    /* 4. Injeta o número do vetor físico final calculado para a IDT */
    uint32_t msi_data = (MSI_VECTOR_BASE + target_slot) & 0xFF; 
    pci_config_write_dword(bus, slot, function, data_offset, msi_data);

    /* 
     * CORREÇÃO 3: Liga o MSI Enable (Bit 0 do Message Control, que mapeia para o Bit 16 do DWORD) 
     * Fazemos a leitura e escrita de forma totalmente alinhada a 32 bits no offset inicial.
     */
    cap_reg = pci_config_read_dword(bus, slot, function, capp_addr);
    cap_reg |= (1 << 16); /* Liga o bit 0 do Message Control de forma segura */
    pci_config_write_dword(bus, slot, function, capp_addr, cap_reg);

    /* 6. Atualiza o pool dinâmico mestre e o metadado de interrupção do driver */
    fnvetors_handler_msi[target_slot] = (void*)fuc;
    dev->irq_line = MSI_VECTOR_BASE + target_slot; 
    
    return 0;
}

/**
 * Inicializa o subsistema MSI.
 */
void msi_init(void)
{
    for (int i = 0; i < MAX_MSI_PINS; i++)
    {
        fnvetors_handler_msi[i] = NULL;
    }
    
    kprintf("[MSI] Subsistema de interrupcoes por mensagem inicializado.\n");
}