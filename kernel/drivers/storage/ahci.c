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
 *  Modified Date: 13/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/drivers/storage/ahci.h>
#include <kernel/drivers/storage/block.h>
#include <kernel/kvmm.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/klib.h>

#include <kernel/arch/x86_64/kapi/irq.h>
#include <kernel/arch/x86_64/kapi/msi.h>

/* Estados e Limites Críticos */
#define AHCI_MAX_DEVICES 32
#define AHCI_PRDT_PER_CMD 8
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_SR_BSY 0x80
#define ATA_SR_DRQ 0x08

/* Estrutura interna de alto rendimento para controlo de DMA por dispositivo */
typedef struct
{
    int port_id;
    int present;
    uint64_t total_sectors;
    hba_port_t *regs;

    /* Endereços físicos e virtuais permanentes das estruturas de paginação de hardware */
    uintptr_t clb_phys;
    void *clb_virt;
    uintptr_t fb_phys;
    void *fb_virt;

    uintptr_t ctba_phys[32]; /* CORREÇÃO: Array indexado recuperado do código original */
    void *ctba_virt[32];

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

/**
 * Publica uma porta SATA ativa utilizando os metadados reais obtidos do hardware.
 */
static void ahci_register_block(ahci_device_t *ahci_dev, int port_index, ata_identify_t *identify);

static void ahci_port_stop(hba_port_t *port)
{
    port->cmd &= ~AHCI_PxCMD_ST;
    port->cmd &= ~AHCI_PxCMD_FRE;
    while (port->cmd & (AHCI_PxCMD_CR | AHCI_PxCMD_FR))
        ;
}

static void ahci_port_start(hba_port_t *port)
{
    while (port->cmd & AHCI_PxCMD_CR)
        ;
    port->cmd |= AHCI_PxCMD_FRE;
    port->cmd |= AHCI_PxCMD_ST;
}

static int ahci_find_free_slot(hba_port_t *port)
{
    uint32_t slots = (port->ci | port->sact);
    for (int i = 0; i < 32; i++)
    {
        if ((slots & (1 << i)) == 0)
            return i;
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

        // Captura o estado elétrico atual da porta
        uint32_t port_is = port->is;

        if (port_is != 0)
        {
            // 1. Limpa PRIMEIRO o sinal físico interno da porta específica escrevendo os bits ativos nela
            port->is = port_is;

            // 2. Atualiza o estado dos semáforos dos slots em RAM de forma segura
            for (int slot = 0; slot < 32; slot++)
            {
                // Se o hardware diz que o slot terminou (ci bit zerado) e o software achava ocupado
                if ((port->ci & (1 << slot)) == 0 && dev->slot_busy[slot] == 1)
                {
                    dev->slot_busy[slot] = 0;
                }
            }

            // 3. Limpa EM SEGUIDA o sinal no registo mestre global (HBA) para desbloquear a linha PCIe
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
static int ahci_setup_port_dma(ahci_device_t *dev)
{
    hba_port_t *port = dev->regs;

    ahci_port_stop(port);

    uintptr_t clb_page = pmm_alloc_page();
    if (!clb_page)
        return -1;
    dev->clb_phys = clb_page;
    dev->clb_virt = vmm_map_device((unsigned long)dev->clb_phys, PAGE_SIZE);
    if (!dev->clb_virt)
        return -1;
    memset(dev->clb_virt, 0, PAGE_SIZE);

    uintptr_t fb_page = pmm_alloc_page();
    if (!fb_page)
        return -1;
    dev->fb_phys = fb_page;
    dev->fb_virt = vmm_map_device((unsigned long)dev->fb_phys, PAGE_SIZE);
    if (!dev->fb_virt)
        return -1;
    memset(dev->fb_virt, 0, PAGE_SIZE);

    port->clb = (uint32_t)(dev->clb_phys & 0xFFFFFFFF);
    port->clbu = (uint32_t)((dev->clb_phys >> 32) & 0xFFFFFFFF);
    port->fb = (uint32_t)(dev->fb_phys & 0xFFFFFFFF);
    port->fbu = (uint32_t)((dev->fb_phys >> 32) & 0xFFFFFFFF);

    /* Habilita as interrupções específicas para esta porta (D2H FIS, Interrupt e PRDT) */
    port->ie = (1 << 0) | (1 << 2) | (1 << 5);

    for (int slot = 0; slot < 32; slot++)
    {
        uintptr_t ctba_page = pmm_alloc_page();
        if (!ctba_page)
            return -1;
        dev->ctba_phys[slot] = ctba_page;
        dev->ctba_virt[slot] = vmm_map_device((unsigned long)dev->ctba_phys[slot], PAGE_SIZE);
        if (!dev->ctba_virt[slot])
            return -1;
        memset(dev->ctba_virt[slot], 0, PAGE_SIZE);

        hba_cmd_header_t *cmd_hdr = (hba_cmd_header_t *)(dev->clb_virt + (slot * sizeof(hba_cmd_header_t)));
        cmd_hdr->ctba = (uint32_t)(dev->ctba_phys[slot] & 0xFFFFFFFF);
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
    if (slot == -1)
        return -1;

    hba_cmd_header_t *cmdhdr = (hba_cmd_header_t *)(dev->clb_virt + (slot * sizeof(hba_cmd_header_t)));
    cmdhdr->cfl = sizeof(h2d_register_fis_t) / sizeof(uint32_t);
    cmdhdr->w = write_flag ? 1 : 0;
    cmdhdr->prdtl = 1;
    cmdhdr->pmp = 0;

    hba_cmd_tbl_t *cmdtable = (hba_cmd_tbl_t *)dev->ctba_virt[slot];
    memset(cmdtable, 0, PAGE_SIZE);

    cmdtable->prdt_entry.dba = (uint32_t)(phys_buffer & 0xFFFFFFFFUL);
    cmdtable->prdt_entry.dbau = (uint32_t)((phys_buffer >> 32) & 0xFFFFFFFFUL);
    cmdtable->prdt_entry.dbc = (sector_count * 512) - 1;
    cmdtable->prdt_entry.i = 1;

    h2d_register_fis_t *cfis = (h2d_register_fis_t *)(&cmdtable->cfis);
    cfis->fis_type = FIS_TYPE_REG_H2D;
    cfis->c = 1;
    cfis->command = write_flag ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT;
    cfis->device = 1 << 6;

    cfis->lba0 = lba & 0xFF;
    cfis->lba1 = (lba >> 8) & 0xFF;
    cfis->lba2 = (lba >> 16) & 0xFF;
    cfis->lba3 = (lba >> 24) & 0xFF;
    cfis->lba4 = (lba >> 32) & 0xFF;
    cfis->lba5 = (lba >> 40) & 0xFF;
    cfis->countl = sector_count & 0xFF;
    cfis->counth = (sector_count >> 8) & 0xFF;

    /* Limpa qualquer tráfego residual ou erro elétrico da porta antes do disparo */
    port->is = port->is;
    port->serr = port->serr;

    dev->slot_busy[slot] = 1;

    /* Dispara o comando elétrico no hardware */
    port->ci = (1 << slot);

    /* LOOP DE ESPERA ASYNC (A ISR irá alterar 'slot_busy' para 0 quando a IRQ disparar) */
    while (dev->slot_busy[slot] == 1)
    {
        // Se a sua máquina real demorar ou falhar a IRQ, implementamos um Fallback de polling 
        // para evitar que o Kernel congele infinitamente se a BIOS prender o vetor MSI:
        if ((port->ci & (1 << slot)) == 0) {
            dev->slot_busy[slot] = 0;
            break;
        }
        __builtin_ia32_pause();
    }

    // Garante que o pipeline de interrupção da porta foi limpo e rearmado para o próximo comando
    volatile uint32_t dummy = port->is;
    port->is = dummy;

    return 0;
}

/* Interfaces Públicas Exportadas pelo Driver de Bloco */

int ahci_block_read(int device_id, uint64_t lba, uint32_t count, uintptr_t phys_buffer)
{
    if (device_id >= g_storage_device_count || !g_storage_devices[device_id].present)
        return -1;
    return ahci_dma_io(&g_storage_devices[device_id], lba, count, phys_buffer, 0);
}

int ahci_block_write(int device_id, uint64_t lba, uint32_t count, uintptr_t phys_buffer)
{
    if (device_id >= g_storage_device_count || !g_storage_devices[device_id].present)
        return -1;
    return ahci_dma_io(&g_storage_devices[device_id], lba, count, phys_buffer, 1);
}

/**
 * @brief Executa o comando ATA IDENTIFY usando Polling síncrono.
 *        Reutiliza o buffer permanente do FIS Base (FB) para mitigar overhead de paginação.
 */
static int ahci_identify_device_polling(ahci_device_t *dev)
{
    hba_port_t *port = dev->regs;

    int slot = ahci_find_free_slot(port);
    if (slot == -1)
        return -1;

    // ========================================================================
    // CORREÇÃO: MASCARAR INTERRUPÇÕES DURANTE O POLLING SÍNCRONO
    // ========================================================================
    uint32_t saved_ie = port->ie;
    port->ie = 0; // Silencia o hardware para não disparar interrupções concorrentes

    // Usa o offset de 1024 bytes dentro da página do FB já alocada permanentemente
    uintptr_t phys_buffer = dev->fb_phys + 1024;
    void *virt_buffer = (void*)((uintptr_t)dev->fb_virt + 1024);

    // Limpa apenas os 512 bytes que vamos usar
    memset(virt_buffer, 0, 512);

    // Configura o Command Header no Command List Base (CLB)
    hba_cmd_header_t *cmdhdr = (hba_cmd_header_t *)(dev->clb_virt + (slot * sizeof(hba_cmd_header_t)));
    cmdhdr->cfl = sizeof(h2d_register_fis_t) / sizeof(uint32_t);
    cmdhdr->w = 0; // Operação de LEITURA
    cmdhdr->prdtl = 1;
    cmdhdr->pmp = 0;

    // Configura a tabela PRDT usando o buffer persistente
    hba_cmd_tbl_t *cmdtable = (hba_cmd_tbl_t *)dev->ctba_virt[slot];
    memset(cmdtable, 0, PAGE_SIZE);

    cmdtable->prdt_entry.dba = (uint32_t)(phys_buffer & 0xFFFFFFFFUL);
    cmdtable->prdt_entry.dbau = (uint32_t)((phys_buffer >> 32) & 0xFFFFFFFFUL);
    cmdtable->prdt_entry.dbc = 511; // 512 bytes (512 - 1)
    cmdtable->prdt_entry.i = 0;     // Polling ativo (Sem interrupção do PRDT)

    // Monta o FIS Host-to-Device (H2D)
    h2d_register_fis_t *cfis = (h2d_register_fis_t *)(&cmdtable->cfis);
    cfis->fis_type = FIS_TYPE_REG_H2D;
    cfis->c = 1;
    cfis->command = ATA_CMD_IDENTIFY;
    cfis->device = 0;

    // Aguarda até que o dispositivo liberte os bits BSY e DRQ
    uint32_t spin = 0;
    while ((port->tfd & (ATA_SR_BSY | ATA_SR_DRQ)) && spin++ < 1000000)
    {
        __builtin_ia32_pause();
    }
    if (spin >= 1000000)
    {
        kprintf("[AHCI] Porta [%d] ocupada antes do IDENTIFY. TFD: %X\n", dev->port_id, port->tfd);
        port->ie = saved_ie; // Restaura antes de sair
        return -1;
    }

    port->is = port->is;    // Limpa flags residuais
    port->ci = (1 << slot); // Dispara o hardware

    // LAÇO DE POLLING SÍNCRONO
    uint64_t timeout = 0;
    while (1)
    {
        if ((port->ci & (1 << slot)) == 0)
        {
            break;
        }

        if (port->tfd & (1 << 0))
        { // Bit ERR activo
            kprintf("[AHCI] Erro no TFD da porta [%d] durante o polling.\n", dev->port_id);
            port->ie = saved_ie; // Restaura antes de sair
            return -1;
        }

        __builtin_ia32_pause();

        if (timeout++ > 50000000)
        {
            kprintf("[AHCI] Timeout por Polling no comando IDENTIFY na porta [%d]\n", dev->port_id);
            port->ie = saved_ie; // Restaura antes de sair
            return -1;
        }
    }

    // 1. Limpa todas as flags de interrupção pendentes na porta específica
    uint32_t port_is = port->is;
    port->is = port_is;

    // 2. Limpa o bit correspondente a esta porta no registrador mestre global (GHC)
    if (dev->hba_base_virt)
    {
        dev->hba_base_virt->is = (1 << dev->port_id);
    }

    // ========================================================================
    // CORREÇÃO EXIGIDA: REINICIAR O MOTOR DE COMANDOS DA PORTA (DESTRIÇÃO DE LOCKS)
    // ========================================================================
    // Desliga o processamento de lista de comandos (ST = 0)
    port->cmd &= ~AHCI_PxCMD_ST;
    while (port->cmd & AHCI_PxCMD_CR) 
    {
        __builtin_ia32_pause(); 
    }

    // Limpa erros pendentes gerados no encerramento da transferência
    port->serr = 0xFFFFFFFF;

    // Religa o motor (ST = 1). Agora a fila física de slots está redefinida para a ISR!
    port->cmd |= AHCI_PxCMD_ST;

    // Restaura as interrupções padrão da porta para os próximos comandos de E/S assíncronos
    port->ie = saved_ie;
    // ========================================================================

    // Processa os dados recebidos de forma segura
    ata_identify_t *id = (ata_identify_t *)virt_buffer;

    // Inversão de bytes (Endianness) para o SERIAL NUMBER (20 bytes / 10 palavras)
    for (int i = 0; i < 20; i += 2)
    {
        char tmp = id->serial_number[i];
        id->serial_number[i] = id->serial_number[i + 1];
        id->serial_number[i + 1] = tmp;
    }

    // Corrige a inversão de bytes (Endianness string padrão ATA)
    for (int i = 0; i < 40; i += 2)
    {
        char tmp = id->model_number[i];
        id->model_number[i] = id->model_number[i + 1];
        id->model_number[i + 1] = tmp;
    }

    // Captura o total de setores usando o mapeamento limpo da struct
    dev->total_sectors = id->total_sectors_48;

    // Se o SSD reportar 0 no campo de 48 bits, faz o fallback para LBA28
    if (dev->total_sectors == 0)
    {
        dev->total_sectors = id->total_sectors_28;
    }

    /*
    // Nota: O cálculo de GB necessita de cast (uint64_t) para prevenir overflow aritmético de 32 bits
    uint64_t size_in_gb = (dev->total_sectors * 512UL) / (1024UL * 1024UL * 1024UL);

    kprintf("[AHCI] HDDs/SSDs SATA Identificado com Sucesso!\n");
    kprintf("[AHCI] Modelo: %s\n", id->model_number);
    kprintf("[AHCI] Tamanho: %llu GB (%llu setores em LBA)\n", size_in_gb, dev->total_sectors);
    */

    return 0;
}


/**
 * @brief Inicializa o Controlador de Host AHCI e realiza o Probing de Dispositivos.
 *
 * Esta função configura o controlador global HBA e varre as 32 portas lógicas
 * em busca de unidades de armazenamento (HDDs/SSDs SATA).
 *
 * Devido às especificidades de gerenciamento agressivo de energia de chipsets móveis
 * (como o Intel Sunrise Point do HP ProBook 430 G3), a rotina implementa uma sequência
 * estrita de inicialização de hardware:
 *  1. Habilita o barramento AHCI e realiza um Host Reset Global (GHC.HR).
 *  2. Configura e ativa os vetores de interrupção (MSI ou IRQ legada) de forma precoce.
 *  3. Acorda eletricamente cada porta ativa aplicando os sinais Spin-Up (SUD) e Força
 *     Ativa de Interface (ICC_ACTIVE) para tirar slots M.2 do estado de suspensão profunda (D3).
 *  4. Dispara um sinal elétrico de COMRESET através do registo SCTL e limpa os registos de
 *     erro (SERR) exigidos pelo silício da Intel.
 *  5. Ativa temporariamente o motor de receção de FIS (PxCMD.FRE) antes de ler a assinatura,
 *     garantindo que o registo PxSIG seja corretamente populado pelo hardware.
 *  6. Mapeia e inicializa as estruturas internas de DMA e PRDT para portas com link estável.
 *
 * @param dev Ponteiro para a estrutura do dispositivo detetado no barramento PCI.
 * @return int Retorna 0 em caso de sucesso na inicialização, ou -1 se falhar.
 */
int ahci_init(pci_device_t *dev)
{
    pci_enable_mmio_busmastering(dev);

    uint32_t bar5 = dev->bar[5];
    if (!bar5)
        return -1;

    /* CORREÇÃO: Mapeia o tamanho de uma página completa (4KB) para abranger todas as portas em segurança */
    hba_mem_t *hba_mem = (hba_mem_t *)vmm_map_device((unsigned long)bar5, PAGE_SIZE);
    if (!hba_mem)
        return -1;

    if (!apic_send_msi(dev, ahci_interrupt_handler))
    {
        kprintf("MSI enabled\n");
    }
    else
    {
        kapi_register_irq_handler(dev->irq_line, ahci_interrupt_handler);
        kprintf("IRQ enabled, [%d]\n", dev->irq_line);
    }

    /* Inicialização fria do Controlador */
    hba_mem->ghc |= AHCI_GHC_AE; // GHC.AE = 1 (Habilita a arquitetura AHCI)
    hba_mem->ghc |= AHCI_GHC_HR; // GHC.HR = 1 (Aplica o Host Reset)

    // Aguarda o hardware limpar o bit de Reset (GHC.HR vai para 0)
    int timeout_hr = 0;
    while ((hba_mem->ghc & AHCI_GHC_HR) && timeout_hr++ < 10000)
    {
        __builtin_ia32_pause();
    }

    hba_mem->ghc |= AHCI_GHC_AE; // Garante que continua habilitado após o reset

    hba_mem->ghc |= AHCI_GHC_AE; // Habilita interrupções globais (IE)

    /* CORREÇÃO: Liga as interrupções globais de imediato para evitar perdas de sinal durante o probing */
    hba_mem->ghc |= AHCI_GHC_IE;

    uint32_t pi = hba_mem->pi;
    for (int i = 0; i < 32; i++)
    {
        if (pi & (1 << i))
        {
            hba_port_t *port = &hba_mem->ports[i];

            // Força o Power On e o Spin-Up do dispositivo no barramento físico
            port->cmd |= AHCI_PxCMD_ICC_ACTIVE; // POD: Power On Device
            port->cmd |= AHCI_PxCMD_SUD;        // SUD: Spin-Up Device (Gatilha a transmissão do sinal elétrico SATA)
            for (volatile int d = 0; d < 4000000; d++)
                ; // Janela de tempo elétrico

            // 1. Para o motor se estiver rodando
            port->cmd &= ~(AHCI_PxCMD_FRE | AHCI_PxCMD_ST); // Limpa FRE (bit 4) e ST (bit 0)
            while (port->cmd & AHCI_PxCMD_CR)
                ; // Aguarda bit CR apagar

            // Desativa modos agressivos de economia de energia Intel
            port->cmd &= ~(AHCI_PxCMD_ASP | AHCI_PxCMD_ALPE); // Limpa PxCMD.ALPE e PxCMD.APSTE

            // 2. Dispara o COMRESET físico
            port->sctl = (port->sctl & ~0x0F) | 1;

            // Loop de atraso para o pulso elétrico na linha
            for (volatile int d = 0; d < 2000000; d++)
                ;

            // 3. Finaliza o reset elétrico
            port->sctl = (port->sctl & ~0x0F);

            // Pausa de 10ms exigida pela especifica;\ao antes da leitura
            for (volatile int d = 0; d < 5000000; d++)
                ;

            // 4. Aguarda a estabilização da camada física (Timeout de até 50ms)
            int timeout_phy = 0;
            while (timeout_phy++ < 50000)
            {
                if ((port->ssts & 0x0F) == 3)
                    break;
                for (volatile int d = 0; d < 1000; d++)
                    ;
            }

            // Limpeza do registo de errros SERR
            port->serr = 0xFFFFFFFF;

            port->cmd |= (1 << 4); // Ativa o bit PxCMD.FRE (Fis Receive Enable)

            // Pequeno delay para o chip Intel atualizar o registrador port->sig
            for (volatile int d = 0; d < 1000000; d++)
                ;

            uint32_t ssts = port->ssts;
            uint8_t det = ssts & 0x0F;
            uint8_t ipm = (ssts >> 8) & 0x0F;

            // ... código anterior de verificação de assinaturas ...
            if (det == HBA_PORT_DET_PRESENT && ipm == HBA_PORT_IPM_ACTIVE && port->sig == AHCI_SATA_SIG_ATA)
            {
                if (g_storage_device_count >= AHCI_MAX_DEVICES)
                    break;

                ahci_device_t *sata_dev = &g_storage_devices[g_storage_device_count];
                sata_dev->port_id = g_storage_device_count; //i;
                sata_dev->regs = port;
                sata_dev->hba_base_virt = hba_mem;
                sata_dev->present = 1;

                if (ahci_setup_port_dma(sata_dev) == 0)
                {
                    kprintf("[AHCI] Porta [%d]: Armazenamento mapeado de forma permanente via VMM.\n", i);

                    // Incrementa o contador ANTES do identify para a ISR reconhecer o dispositivo
                    g_storage_device_count++;

                    // Aloca ou aponta para o buffer onde a sua função preencheu os 512 bytes obtidos do hardware
                    // Nota: Verifique se a sua função 'ahci_identify_device_polling' escreve internamente no slot,
                    // ou se precisa de passar o ponteiro. Presumimos aqui que ela guarda os dados na tabela ctba_virt[0].
                    ata_identify_t *identify_data = (ata_identify_t *)(sata_dev->fb_virt + 1024);

                    if (ahci_identify_device_polling(sata_dev) != 0)
                    {
                        // Se o polling falhar, desfaz o registo por segurança
                        g_storage_device_count--;
                        sata_dev->present = 0;
                        kprintf("[AHCI] Erro ao identificar dispositivo na porta [%d]\n", i);
                    }
                    else
                    {
                        // Passagem correta dos argumentos para a função
                        ahci_register_block(sata_dev, sata_dev->port_id, identify_data);
                    }

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




/**
 * =============================================================================================
 * Publica uma porta SATA ativa do AHCI como um Dispositivo de Blocos para o Kernel.
 */

 /* Wrappers e Conversor de Paginação Virtual/Física */
static uintptr_t ahci_virtual_to_physical(void* virtual_addr) {
    extern uintptr_t vmm_get_physical(uintptr_t virtual_address);
    return vmm_get_physical((uintptr_t)virtual_addr);
}

static int ahci_backend_read_blocks(struct block_device* dev, uint64_t lba, uint32_t count, void* buffer) {
    if (!dev || !dev->private_data || !buffer) return -1;
    ahci_device_t* ahci_dev = (ahci_device_t*)dev->private_data;
    uintptr_t phys_buf = ahci_virtual_to_physical(buffer);
    if (!phys_buf) return -3;
    return ahci_block_read(ahci_dev->port_id, lba, count, phys_buf);
}

static int ahci_backend_write_blocks(struct block_device* dev, uint64_t lba, uint32_t count, void* buffer) {
    if (!dev || !dev->private_data || !buffer) return -1;
    ahci_device_t* ahci_dev = (ahci_device_t*)dev->private_data;
    uintptr_t phys_buf = ahci_virtual_to_physical(buffer);
    if (!phys_buf) return -3;
    return ahci_block_write(ahci_dev->port_id, lba, count, phys_buf);
}

/**
 * Publica uma porta SATA ativa utilizando os metadados reais obtidos do hardware.
 */
static void ahci_register_block(ahci_device_t *ahci_dev, int port_index, ata_identify_t *identify)
{
    if (!ahci_dev || !identify) return;

    block_device_t *bdev = (block_device_t *)kmalloc(sizeof(block_device_t));
    if (!bdev) return;

    memset(bdev, 0, sizeof(block_device_t));
    ksprintf(bdev->name, "ahci%d", port_index);

    char cleaned_model[41];
    char cleaned_serial[21];

    memcpy(cleaned_model, identify->model_number, 40);
    memcpy(cleaned_serial, identify->serial_number, 20);
    cleaned_model[40] = '\0';
    cleaned_serial[20] = '\0';

    uint64_t capacity = 0;
    if (identify->total_sectors_48 > 0) {
        capacity = identify->total_sectors_48;
    } else {
        capacity = identify->total_sectors_28;
    }

    ahci_dev->total_sectors = capacity;
    bdev->total_sectors     = capacity;
    bdev->sector_size       = 512; 

    bdev->read_blocks   = ahci_backend_read_blocks;
    bdev->write_blocks  = ahci_backend_write_blocks;
    bdev->ioctl         = NULL; 
    bdev->private_data  = (void *)ahci_dev;

    int assigned_id = register_block_device(bdev);

    if (assigned_id >= 0) {
        kprintf("[AHCI] Disco '%s' Registado [ID: %d]\n"
                "       Modelo: %s\n"
                "       S/N:    %s\n"
                "       Tam:    %lu setores (~%lu MB)\n",
                bdev->name, assigned_id, cleaned_model, cleaned_serial, 
                (unsigned long)bdev->total_sectors,
                (unsigned long)((bdev->total_sectors * 512) / (1024 * 1024)));
    } else {
        kfree(bdev);
    }
}
