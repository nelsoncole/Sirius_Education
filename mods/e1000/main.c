/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: main.c
 *    Description: Módulo Driver Dinâmico (LKM) polimórfico para a placa Intel e1000.
 *                 Suporta Mapeamento MMIO, Alocação Pool DMA, Interrupções MSI/INTx
 *                 e suporte unificado para os IDs: 0x1004, 0x100e, 0x100f, 0x153A, 0x10EA.
 * 
 *         Author: Nelson Cole
 *   Created Date: 19/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 10/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kmods/kapi.h>
#include "e1000.h"

/* Variáveis de Controlo de Estado do Driver */
static unsigned long base_addr;
static int rx_cur;
static int tx_cur;
static uint16_t global_device_id;

struct e1000_rx_desc *rx_descs[E1000_NUM_RX_DESC];
struct e1000_tx_desc *tx_descs[E1000_NUM_TX_DESC];

struct e1000_rx_memory rx_memory;
struct e1000_rx_memory tx_memory;

/* Protótipos das funções do módulo */
int e1000_send_package(const void* buffer, uint32_t length);
void e1000_recieve_package(void);
void e1000_enable_int(void);
void e1000_link_up(void);

/* Leituras e escritas diretas mapeadas na MMIO */
unsigned int e1000_read_command(unsigned short addr)
{
    return *(volatile unsigned int *)(base_addr + addr);
}

void e1000_write_command(unsigned short addr, unsigned int val)
{
    *(volatile unsigned int *)(base_addr + addr) = val;
}

/* Leitura acelerada da EEPROM com timeout ultra-rápido para evitar lentidão */
uint16_t e1000_eeprom_read(uint32_t offset)
{
    uint32_t eecd = e1000_read_command(0x10);
    e1000_write_command(0x10, eecd | (1 << 4));
    for (volatile int i = 0; i < 50; i++) { __asm__ __volatile__("pause;"); }
    
    e1000_write_command(REG_EERD, (offset << 8) | 1);
    
    volatile uint32_t timeout = 1000;
    while (!(e1000_read_command(REG_EERD) & 0x10))
    {
        if (--timeout == 0)
        {
            kprintf("[E1000] Erro: Timeout na leitura da EEPROM no offset %d!\n", offset);
            return 0;
        }
        __asm__ __volatile__("pause;");
    }
    
    return (uint16_t)(e1000_read_command(REG_EERD) >> 16);
}

/* Escrita e leitura no transcetor físico PHY */
void e1000_write_phy(uint32_t reg, uint16_t val)
{
    e1000_write_command(REG_MDIC, ((reg & 0x1F) << 16) | (1 << 21) | (1 << 26) | val);
    volatile uint32_t timeout = 1000;
    while (!(e1000_read_command(REG_MDIC) & (1 << 28)))
    {
        if (--timeout == 0) return;
        __asm__ __volatile__("pause;");
    }
}

uint16_t e1000_read_phy(uint32_t reg)
{
    e1000_write_command(REG_MDIC, ((reg & 0x1F) << 16) | (1 << 21) | (2 << 26));
    volatile uint32_t timeout = 1000;
    while (!(e1000_read_command(REG_MDIC) & (1 << 28)))
    {
        if (--timeout == 0) return 0;
        __asm__ __volatile__("pause;");
    }
    return (uint16_t)(e1000_read_command(REG_MDIC) & 0xFFFF);
}

static int e1000_hal_transmit(const void* buffer, uint32_t length)
{
    return e1000_send_package(buffer, length);
}

static int e1000_hal_receive(const void* buffer, uint32_t length)
{
    (void)buffer; (void)length;
    return 0;
}

static int e1000_hal_open(void)
{
    e1000_enable_int();
    e1000_link_up();
    return 0;
}

static int e1000_hal_stop(void)
{
    e1000_write_command(REG_RCTRL, 0);
    e1000_write_command(REG_TCTRL, 0);
    return 0;
}

/* ESTRUTURA POLIMÓRFICA DO DRIVER DE REDE */
typedef struct 
{
    int (*transmit)(const void* buffer, uint32_t length);
    int (*receive)(const void* buffer, uint32_t length); 
    int (*open)(void);
    int (*stop)(void);
} net_device_ops_t;

static net_device_ops_t e1000_ops = {
    .transmit = e1000_hal_transmit,
    .receive  = e1000_hal_receive,
    .open     = e1000_hal_open,
    .stop     = e1000_hal_stop
};

unsigned char e1000_is_eeprom(void)
{
    for (int i = 0; i < 1000; ++i)
    {
        unsigned long to = e1000_read_command(0x14);
        if ((to & 0x10) == 0x10) return 1;
    }
    return 0;
}

void e1000_enable_int(void)
{
    e1000_write_command(0xD0, 0x1F6DC);
    e1000_write_command(0xD0, 0xff & ~4);
    e1000_read_command(0xC0);
    kprintf("[E1000]: Interrupts enabled!\n");
}

void e1000_link_up(void)
{
    if (global_device_id != 0x100e && global_device_id != 0x1004) 
    {
        uint16_t phy_ctrl = e1000_read_phy(0);
        e1000_write_phy(0, phy_ctrl | 0x1200); 
        unsigned int ctrl = e1000_read_command(0);
        e1000_write_command(0, ctrl | 0x40 | 0x20); 
    }
    else 
    {
        unsigned int ty = e1000_read_command(0);
        e1000_write_command(0, ty | 0x40);
    }
    kprintf("[E1000]: Link is up!\n");
}

/* ISR Universal protegida contra Interrupt Storm (Clear-on-read REG_ICR) */
void irq_e1000(void)
{
    unsigned int to = e1000_read_command(REG_ICR);

    if (to == 0) return;

    if (to & 0x01) { /* Transmit completed */ }
    if (to & 0x02) { /* Transmit queue empty */ }
    if (to & 0x04) {
        unsigned int status = e1000_read_command(REG_STATUS);
        if (status & STATUS_LU)
        {
            /* Link está UP - Cabo Conectado */
            e1000_link_up();
            net_set_link_event(NET_EVENT_LINK_UP);
        }
        else
        {
            /* Link está DOWN - Cabo Desconectado */
            net_set_link_event(NET_EVENT_LINK_DOWN);
        } 
    }
    if (to & 0x80) { e1000_recieve_package(); }
}

int e1000_init(pci_device_t *dev)
{
    unsigned char mac_address[6];
    global_device_id = dev->device_id;

    uint32_t bar0 = dev->bar[0];
    if (!bar0) return -1;

    kprintf("[E1000] Device ID detetado: 0x%x\n", global_device_id);
    kprintf("[E1000] Base physical address: 0x%x \n", bar0);

    base_addr = (unsigned long)vmm_map_device((unsigned long)bar0, 0x400000);
    if (!base_addr)
    {
        kprintf("[E1000] Error: MMIO mapping failed\n");
        return -1;
    }

    // 1. HARDWARE RESET IMEDIATO PÓS-MAP MMIO
    if (global_device_id != 0x100e && global_device_id != 0x1004)
    {
        e1000_write_command(0, e1000_read_command(0) | 0x04000000); 
        volatile uint32_t rst_timeout = 500000;
        while (e1000_read_command(0) & 0x04000000)
        {
            if (--rst_timeout == 0)
            {
                kprintf("[E1000] Erro: Timeout no Reset de Hardware!\n");
                break;
            }
            __asm__ __volatile__("pause;");
        }
        for(volatile int i = 0; i < 50000; i++) { __asm__ __volatile__("pause;"); }
        e1000_write_command(REG_PBA, 0x0030); 
    }

    // 2. FORÇA A LIMPEZA TOTAL DE MÁSCARAS INTERNAS (ESSENCIAL PARA DISPARAR IRQ)
    // Grava 0xFFFFFFFF no registo 0xD8 (REG_IMC - Interrupt Mask Clear) para desimpedir o chip
    e1000_write_command(0xD8, 0xFFFFFFFF);
    e1000_read_command(0xC0); // Limpa o ICR inicial

    // 3. ALOCAÇÃO DOS POOLS DE MEMÓRIA DMA
    size_t rx_size = 98 * 4096;
    rx_memory.vmem = (uintptr_t)pool_alloc(rx_size);
    if (!rx_memory.vmem) return -2;
    
    rx_memory.phymem = vmm_get_physical(rx_memory.vmem) & 0x7FFFFFFFFFFFLL;
    rx_memory.descsize = 0x2000;
    rx_memory.blocksize = 0x3000;
    rx_memory.start = rx_memory.vmem + rx_memory.descsize;

    size_t tx_size = 18 * 4096;
    tx_memory.vmem = (uintptr_t)pool_alloc(tx_size);
    if (!tx_memory.vmem)
    {
        pool_free((void *)rx_memory.vmem, rx_size);
        return -3;
    }
    tx_memory.phymem = vmm_get_physical(tx_memory.vmem) & 0x7FFFFFFFFFFFLL;
    tx_memory.descsize = 0x2000;
    tx_memory.blocksize = 0x2000;
    tx_memory.start = tx_memory.vmem + tx_memory.descsize;

    memset((char *)rx_memory.vmem, 0, rx_size);
    memset((char *)tx_memory.vmem, 0, tx_size);

    // 4. EXTRAÇÃO HÍBRIDA DO ENDEREÇO MAC
    bool mac_resolved = false;
    if (global_device_id != 0x100e && global_device_id != 0x1004) 
    {
        uint16_t val0 = e1000_eeprom_read(0);
        uint16_t val1 = e1000_eeprom_read(1);
        uint16_t val2 = e1000_eeprom_read(2);

        if (val0 != 0 || val1 != 0 || val2 != 0)
        {
            mac_address[0] = val0 & 0xFF; mac_address[1] = val0 >> 8;
            mac_address[2] = val1 & 0xFF; mac_address[3] = val1 >> 8;
            mac_address[4] = val2 & 0xFF; mac_address[5] = val2 >> 8;
            mac_resolved = true;
        }
    }

    if (!mac_resolved)
    {
        unsigned int data = e1000_read_command(0x5400);
        mac_address[0] = ((data & 0x000000FF) >> 0) & 0xFF;
        mac_address[1] = ((data & 0x0000FF00) >> 8) & 0xFF;
        mac_address[2] = ((data & 0x00FF0000) >> 16) & 0xFF;
        mac_address[3] = ((data & 0xFF000000) >> 24) & 0xFF;
        data = e1000_read_command(0x5400 + 4);
        mac_address[4] = ((data & 0x000000FF) >> 0) & 0xFF;
        mac_address[5] = ((data & 0x0000FF00) >> 8) & 0xFF;
    }

    for (int i = 0; i < 0x80; i++) e1000_write_command(0x5200 + (i * 4), 0);

    // Força a ativação estável das propriedades de Link no registo CTRL (0x00)
    unsigned int ctrl_val = e1000_read_command(0x00);
    e1000_write_command(0x00, ctrl_val | 0x40 | 0x20);

    // 5. SETUP DO ANEL DE DESCRITORES Rx
    struct e1000_rx_desc *descs = (struct e1000_rx_desc *)rx_memory.vmem;
    for (int i = 0; i < E1000_NUM_RX_DESC; i++)
    {
        rx_descs[i] = descs++;
        unsigned long long addr = rx_memory.phymem + rx_memory.descsize + (rx_memory.blocksize * i);
        rx_descs[i]->addr_1 = addr;
        rx_descs[i]->addr_2 = addr >> 32;
        rx_descs[i]->status = 0;
        rx_descs[i]->length = 8192;
    }

    e1000_write_command(REG_RDBAL, (unsigned int)rx_memory.phymem);
    e1000_write_command(REG_RDBAH, (unsigned int)(rx_memory.phymem >> 32));
    e1000_write_command(REG_RDLEN, E1000_NUM_RX_DESC * 16);
    e1000_write_command(REG_RDH, 0);
    e1000_write_command(REG_RDT, E1000_NUM_RX_DESC - 1);

    unsigned int rctl = RCTL_EN | RCTL_SBP | RCTL_BAM | RCTL_SECRC | RCTL_UPE | RCTL_MPE | RCTL_LBM_NONE | RTCL_RDMTS_HALF | RCTL_BSIZE_8192;
    e1000_write_command(REG_RCTRL, rctl);
    e1000_write_command(REG_RDTR, 0);
    rx_cur = 0;

    // 6. SETUP DO ANEL DE DESCRITORES Tx
    struct e1000_tx_desc *descs2 = (struct e1000_tx_desc *)tx_memory.vmem;
    for (int i = 0; i < E1000_NUM_TX_DESC; i++)
    {
        tx_descs[i] = descs2++;
        unsigned long long addr = tx_memory.phymem + tx_memory.descsize + (tx_memory.blocksize * i);
        tx_descs[i]->addr_1 = addr;
        tx_descs[i]->addr_2 = addr >> 32;
        tx_descs[i]->cmd = 0;
        tx_descs[i]->length = 0;
        tx_descs[i]->status = TSTA_DD;
    }

    e1000_write_command(REG_TDBAL, (unsigned int)tx_memory.phymem);
    e1000_write_command(REG_TDBAH, (unsigned int)(tx_memory.phymem >> 32));
    e1000_write_command(REG_TDLEN, E1000_NUM_TX_DESC * 16);
    e1000_write_command(REG_TDH, 0);
    e1000_write_command(REG_TDT, 0);
    e1000_write_command(0x3828, (0x01000000 | 0x003F0000));
    e1000_write_command(REG_TCTRL, (TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT) | (64 << TCTL_COLD_SHIFT) | TCTL_RTLC));
    e1000_write_command(REG_TIPG, 0x0060200A);
    tx_cur = 0;

    // 7. ATIVAÇÃO DAS MÁSCARAS DE INTERRUPÇÃO (IMS) NO FINAL DO SETUP
    e1000_write_command(0xD0, 0x1F6DC); // 0xD0 = REG_IMS (Ativa os filtros Rx/Tx/Link)
    e1000_read_command(0xC0);           // Limpa o ICR pós-setup para libertar a linha

    pci_enable_mmio_busmastering(dev);
    kapi_register_irq_handler(dev, irq_e1000);

    e1000_link_up();
    net_driver_register(mac_address, &e1000_ops);
    net_set_link_event(NET_EVENT_LINK_UP);
    return 0;
}

int e1000_send_package(const void *buffer, uint32_t length)
{
    unsigned long long dest = tx_memory.start + (tx_memory.blocksize * tx_cur);
    if (length > tx_memory.blocksize)
        return 2;
    memcpy((char *)dest, buffer, length);
    tx_descs[tx_cur]->length = length;
    tx_descs[tx_cur]->cmd = CMD_EOP | CMD_IFCS | CMD_RS;
    tx_descs[tx_cur]->status = 0;
    unsigned char old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write_command(REG_TDT, tx_cur);
    while (!(tx_descs[old_cur]->status & 0xff))
    {
        asm volatile("pause;");
    }
    return 0;
}
void e1000_recieve_package(void)
{
    while ((rx_descs[rx_cur]->status & 0x1))
    {
        size_t old_cur = rx_cur;
        rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
        unsigned long long addr = rx_memory.start + (rx_memory.blocksize * old_cur);
        unsigned short len = rx_descs[old_cur]->length;
        net_driver_receive((const void *)addr, (uint32_t)len);
        rx_descs[old_cur]->status &= ~1;
        e1000_write_command(REG_RDT, old_cur);
    }
}
int module_init(pci_device_t *dev) { return e1000_init(dev); }
void module_exit(void)
{
    e1000_hal_stop();
    pool_free((void *)rx_memory.vmem, 98 * 4096);
    pool_free((void *)tx_memory.vmem, 18 * 4096);
}