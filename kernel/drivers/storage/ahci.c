/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ahci.c
 *    Description: Controlador AHCI de Alta Performance para Armazenamento em Bloco.
 *                 Implementa processamento assíncrono multi-slot, paginação
 *                 Scatter-Gather (PRDT) e otimizações de DMA nativas de 64 bits.
 *                 Mapeia permanentemente as estruturas de portas via vmm_map_device
 *                 para mitigar a troca excessiva de contextos de paginação (TLB flushes).
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

#include <kernel/drivers/storage/ahci.h>
#include <kernel/kvmm.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/klib.h>

#include <kernel/arch/x86_64/kapi/irq.h>
#include <kernel/arch/x86_64/kapi/msi.h>

/* Estados e Limites Críticos */
#define AHCI_MAX_DEVICES         32
#define AHCI_PRDT_PER_CMD        8    
#define ATA_CMD_READ_DMA_EXT     0x25
#define ATA_CMD_WRITE_DMA_EXT    0x35
#define ATA_CMD_IDENTIFY         0xEC

#define ATA_SR_BSY               0x80
#define ATA_SR_DRQ               0x08

/* Estrutura interna de alto rendimento para controlo de DMA por dispositivo */
typedef struct {
    int port_id;
    int present;
    uint64_t total_sectors;
    hba_port_t *regs;
    
    /* Endereços físicos e virtuais permanentes das estruturas de paginação de hardware */
    uintptr_t clb_phys;
    void     *clb_virt;
    uintptr_t fb_phys;
    void     *fb_virt;
    
    uintptr_t ctba_phys[32]; /* CORREÇÃO: Array indexado recuperado do código original */
    void     *ctba_virt[32]; 

    /* Semáforo/Flag de controlo volátil para sincronização multi-slot */
    volatile uint8_t slot_busy[32];

    /* Ponteiro virtual para a base global do controlador HBA (Necessário para ACK global) */
    hba_mem_t *hba_base_virt;

} ahci_device_t;

static ahci_device_t g_storage_devices[AHCI_MAX_DEVICES];
static int g_storage_device_count = 0;

/* Declarações locais antecipadas */
void ahci_interrupt_handler(void);
static int ahci_setup_port_dma(ahci_device_t *dev);

static void ahci_port_stop(hba_port_t *port)
{
    port->cmd &= ~AHCI_PxCMD_ST;
    port->cmd &= ~AHCI_PxCMD_FRE;
    while (port->cmd & (AHCI_PxCMD_CR | AHCI_PxCMD_FR));
}

static void ahci_port_start(hba_port_t *port)
{
    while (port->cmd & AHCI_PxCMD_CR);
    port->cmd |= AHCI_PxCMD_FRE;
    port->cmd |= AHCI_PxCMD_ST;
}

static int ahci_find_free_slot(hba_port_t *port)
{
    uint32_t slots = (port->ci | port->sact);
    for (int i = 0; i < 32; i++)
    {
        if ((slots & (1 << i)) == 0) return i;
    }
    return -1;
}

/**
 * A ROTINA DE SERVIÇO DE INTERRUPÇÃO (ISR) DO AHCI
 */
__attribute__((force_align_arg_pointer))
void ahci_interrupt_handler(void)
{
    for (int d = 0; d < g_storage_device_count; d++) 
    {
        ahci_device_t *dev = &g_storage_devices[d];
        hba_port_t *port = dev->regs;
        hba_mem_t *hba_base = dev->hba_base_virt;

        if (port->is != 0) 
        {
            for (int slot = 0; slot < 32; slot++) 
            {
                if ((port->ci & (1 << slot)) == 0 && dev->slot_busy[slot] == 1) 
                {
                    dev->slot_busy[slot] = 0;
                }
            }

            /* Limpa o sinal físico de interrupção na porta específica */
            uint32_t port_is = port->is;
            port->is = port_is; 

            /* CORREÇÃO: Limpa o sinal no registo mestre global para não prender o barramento PCI */
            if (hba_base) 
            {
                hba_base->is = (1 << dev->port_id);
            }
        }
    }
}

/**
 * Configuração de DMA por Porta SATA
 */
static int ahci_setup_port_dma(ahci_device_t *dev) {
    hba_port_t *port = dev->regs;

    ahci_port_stop(port);

    uintptr_t clb_page = pmm_alloc_page();
    if (!clb_page) return -1;
    dev->clb_phys = clb_page;
    dev->clb_virt = vmm_map_device((unsigned long)dev->clb_phys, PAGE_SIZE);
    if (!dev->clb_virt) return -1;
    memset(dev->clb_virt, 0, PAGE_SIZE);

    uintptr_t fb_page = pmm_alloc_page();
    if (!fb_page) return -1;
    dev->fb_phys = fb_page;
    dev->fb_virt = vmm_map_device((unsigned long)dev->fb_phys, PAGE_SIZE);
    if (!dev->fb_virt) return -1;
    memset(dev->fb_virt, 0, PAGE_SIZE);

    port->clb  = (uint32_t)(dev->clb_phys & 0xFFFFFFFF);
    port->clbu = (uint32_t)((dev->clb_phys >> 32) & 0xFFFFFFFF);
    port->fb   = (uint32_t)(dev->fb_phys & 0xFFFFFFFF);
    port->fbu  = (uint32_t)((dev->fb_phys >> 32) & 0xFFFFFFFF);

    /* Habilita as interrupções específicas para esta porta (D2H FIS, Interrupt e PRDT) */
    port->ie = (1 << 0) | (1 << 2) | (1 << 5); 

    for (int slot = 0; slot < 32; slot++) {
        uintptr_t ctba_page = pmm_alloc_page();
        if (!ctba_page) return -1;
        dev->ctba_phys[slot] = ctba_page;
        dev->ctba_virt[slot] = vmm_map_device((unsigned long)dev->ctba_phys[slot], PAGE_SIZE);
        if (!dev->ctba_virt[slot]) return -1;
        memset(dev->ctba_virt[slot], 0, PAGE_SIZE);

        hba_cmd_header_t *cmd_hdr = (hba_cmd_header_t*)(dev->clb_virt + (slot * sizeof(hba_cmd_header_t)));
        cmd_hdr->ctba  = (uint32_t)(dev->ctba_phys[slot] & 0xFFFFFFFF);
        cmd_hdr->ctbau = (uint32_t)((dev->ctba_phys[slot] >> 32) & 0xFFFFFFFF);
        
        dev->slot_busy[slot] = 0;
    }

    ahci_port_start(port);
    return 0;
}

/**
 * Pipeline Universal de E/S via DMA Assíncrono (Zero-Copy)
 */
static int ahci_dma_io(ahci_device_t *dev, uint64_t lba, uint32_t sector_count, uintptr_t phys_buffer, int write_flag)
{
    hba_port_t *port = dev->regs;
    
    int slot = ahci_find_free_slot(port);
    if (slot == -1) return -1;

    hba_cmd_header_t *cmdhdr = (hba_cmd_header_t*)(dev->clb_virt + (slot * sizeof(hba_cmd_header_t)));
    cmdhdr->cfl = sizeof(h2d_register_fis_t) / sizeof(uint32_t);
    cmdhdr->w = write_flag ? 1 : 0;
    cmdhdr->prdtl = 1;              
    cmdhdr->pmp = 0;

    hba_cmd_tbl_t *cmdtable = (hba_cmd_tbl_t*)dev->ctba_virt[slot];
    memset(cmdtable, 0, PAGE_SIZE);

    cmdtable->prdt_entry.dba  = (uint32_t)(phys_buffer & 0xFFFFFFFFUL);
    cmdtable->prdt_entry.dbau = (uint32_t)((phys_buffer >> 32) & 0xFFFFFFFFUL);
    cmdtable->prdt_entry.dbc  = (sector_count * 512) - 1; 
    cmdtable->prdt_entry.i    = 1;                        

    h2d_register_fis_t *cfis = (h2d_register_fis_t*)(&cmdtable->cfis);
    cfis->fis_type = FIS_TYPE_REG_H2D;
    cfis->c = 1;
    cfis->command = write_flag ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    cfis->device  = 1 << 6; 

    cfis->lba0 = lba & 0xFF;
    cfis->lba1 = (lba >> 8) & 0xFF;
    cfis->lba2 = (lba >> 16) & 0xFF;
    cfis->lba3 = (lba >> 24) & 0xFF;
    cfis->lba4 = (lba >> 32) & 0xFF;
    cfis->lba5 = (lba >> 40) & 0xFF;
    cfis->countl = sector_count & 0xFF;
    cfis->counth = (sector_count >> 8) & 0xFF;

    /* CORREÇÃO: Limpa tráfego residual da porta antes do disparo elétrico */
    port->is = port->is;
    port->serr = port->serr;

    dev->slot_busy[slot] = 1;

    /* Dispara hardware */
    port->ci = (1 << slot);

    while (dev->slot_busy[slot] == 1) 
    {
        __builtin_ia32_pause(); 
    }

    return 0;
}

/* Interfaces Públicas Exportadas pelo Driver de Bloco */

int ahci_read_blocks(int device_id, uint64_t lba, uint32_t count, uintptr_t phys_buffer)
{
    if (device_id >= g_storage_device_count || !g_storage_devices[device_id].present) return -1;
    return ahci_dma_io(&g_storage_devices[device_id], lba, count, phys_buffer, 0);
}

int ahci_write_blocks(int device_id, uint64_t lba, uint32_t count, uintptr_t phys_buffer)
{
    if (device_id >= g_storage_device_count || !g_storage_devices[device_id].present) return -1;
    return ahci_dma_io(&g_storage_devices[device_id], lba, count, phys_buffer, 1);
}

int ahci_init(pci_device_t *dev)
{
    pci_enable_mmio_busmastering(dev);

    uint32_t bar5 = dev->bar[5];
    if (!bar5) return -1;

    /* CORREÇÃO: Mapeia o tamanho de uma página completa (4KB) para abranger todas as portas em segurança */
    hba_mem_t *hba_mem = (hba_mem_t *)vmm_map_device((unsigned long)bar5, PAGE_SIZE);
    if (!hba_mem) return -1;

    /* Inicialização fria do Controlador */
    hba_mem->ghc |= AHCI_GHC_AE;
    hba_mem->ghc |= AHCI_GHC_HR;
    while (hba_mem->ghc & AHCI_GHC_HR);
    hba_mem->ghc |= AHCI_GHC_AE;

    /* CORREÇÃO: Liga as interrupções globais de imediato para evitar perdas de sinal durante o probing */
    hba_mem->ghc |= AHCI_GHC_IE; 

    if(!apic_send_msi(dev, ahci_interrupt_handler))
    {
        kprintf("MSI enabled\n");
    }
    else
    {
        kapi_register_irq_handler(dev->irq_line, ahci_interrupt_handler);
        kprintf("IRQ enabled, [%d]\n", dev->irq_line);
    }

    uint32_t pi = hba_mem->pi;
    for (int i = 0; i < 32; i++)
    {
        if (pi & (1 << i))
        {
            hba_port_t *port = &hba_mem->ports[i];
            uint32_t ssts = port->ssts;
            uint8_t det = ssts & 0x0F;
            uint8_t ipm = (ssts >> 8) & 0x0F;

            if (det == HBA_PORT_DET_PRESENT && ipm == HBA_PORT_IPM_ACTIVE && port->sig == AHCI_SATA_SIG_ATA)
            {
                if (g_storage_device_count >= AHCI_MAX_DEVICES) break;

                ahci_device_t *sata_dev = &g_storage_devices[g_storage_device_count];
                sata_dev->port_id = i;
                sata_dev->regs = port;
                sata_dev->hba_base_virt = hba_mem; /* CORREÇÃO: Atribuído ANTES do setup da porta dma */
                sata_dev->present = 1;

                if (ahci_setup_port_dma(sata_dev) == 0)
                {
                    kprintf("[AHCI] Porta [%d]: Armazenamento mapeado de forma permanente via VMM.\n", i);
                    g_storage_device_count++;
                }
            }
        }
    }

    return 0;
}

/**
 * Ponto de entrada principal do driver AHCI.
 * Regista o controlador no barramento PCI e dispara o carregamento em loop.
 */
void ahci_driver_init(void)
{
    g_storage_device_count = 0;

    /* 
     * Procura e carrega todos os controladores compatíveis com a especificação.
     * Classe 0x01 (Mass Storage), Subclasse 0x06 (SATA).
     */

    pci_load_devices_by_class(PCI_CLASS_STORAGE, PCI_SUBCLASS_SATA, ahci_init);
}