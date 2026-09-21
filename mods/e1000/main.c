/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: main.c
 *    Description: Módulo Driver Dinâmico (LKM) para a placa de rede Intel e1000.
 *                 Suporta Mapeamento MMIO, Alocação Pool DMA, Interrupções MSI
 *                 e integração com a pilha de Rede do Kernel.
 * 
 *         Author: Nelson Cole
 *   Created Date: 19/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 20/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kmods/kapi.h>
#include "e1000.h"

#define PCI_CLASS_NETWORK       0x02
#define PCI_SUBCLASS_ETHERNET   0x00

/* Variáveis de Controlo de Estado do Driver */
static unsigned long base_addr;
static int rx_cur;
static int tx_cur;

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

/* 
 * Encapsulador de Transmissão: Traduz a assinatura pura da HAL do Kernel 
 * para a estrutura de descritores de pacotes da Intel e1000.
 */
static int e1000_hal_transmit(const void* buffer, uint32_t length)
{
    
    return e1000_send_package(buffer, length);
}

/* 
 * Encapsulador de Receção: Polling genérico tolerante da HAL
 */
static int e1000_hal_receive(const void* buffer, uint32_t length)
{
    (void)buffer;
    (void)length;
    return 0;
}

/* Funções de controlo de estado do hardware */
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
        if ((to & 0x10) == 0x10)
        {
            return 1;
        }
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
    unsigned int ty = e1000_read_command(0);
    e1000_write_command(0, ty | 0x40);
    kprintf("[E1000]: Link is up!\n");
}

/*
 * Rotina de Tratamento de Interrupção MSI do Módulo
 */
void irq_e1000(void)
{
    unsigned int to = e1000_read_command(REG_ICR);
    e1000_write_command(REG_IMASK, to);

    if (to & 0x01)
    {
        //kprintf("[E1000]: Transmit completed!\n");
    }
    else if (to & 0x02)
    {
        //kprintf("[E1000]: Transmit queue empty!\n");
    }
    else if (to & 0x04)
    {
        //kprintf("[E1000]: Link change!\n");
        unsigned long ty = e1000_read_command(0);
        e1000_write_command(0, ty | 0x40);
    }
    else if (to & 0x80)
    {
        //kprintf("[E1000]: Package recieved!\n");
        e1000_recieve_package(); /* Dispara a triagem e o net_driver_receive */
    }
    else if (to & 0x10)
    {
        //kprintf("[E1000]: Good threshold!\n");
    }
}

/*
 * Inicialização e Configuração Física da Intel e1000
 */
int e1000_init(pci_device_t *dev);
int module_init(void)
{
    pci_load_devices_by_class(PCI_CLASS_NETWORK, PCI_SUBCLASS_ETHERNET, (void*)e1000_init);
    return 0;
}

int e1000_init(pci_device_t *dev)
{
    unsigned char mac_address[6];

    uint32_t bar0 = dev->bar[0];
    if (!bar0)
    {
        return -1;
    }

    kprintf("[E1000] Base physical address: 0x%x \n", bar0);

    pci_enable_mmio_busmastering(dev);
    kapi_register_irq_handler(dev, irq_e1000);

    base_addr = (unsigned long)vmm_map_device((unsigned long)bar0, 0x400000);
    if (!base_addr)
    {
        kprintf("[E1000] Error: MMIO mapping failed\n");
        return -1;
    }

    size_t rx_size = 98 * 4096;
    rx_memory.vmem = (uintptr_t)pool_alloc(rx_size);
    if (!rx_memory.vmem)
    {
        return -2;
    }
    
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

    unsigned int data = e1000_read_command(0x5400);
    mac_address[0] = ((data & 0x000000FF) >> 0) & 0xFF;
    mac_address[1] = ((data & 0x0000FF00) >> 8) & 0xFF;
    mac_address[2] = ((data & 0x00FF0000) >> 16) & 0xFF;
    mac_address[3] = ((data & 0xFF000000) >> 24) & 0xFF;
    data = e1000_read_command(0x5400 + 4);
    mac_address[4] = ((data & 0x000000FF) >> 0) & 0xFF;
    mac_address[5] = ((data & 0x0000FF00) >> 8) & 0xFF;

    for (int i = 0; i < 0x80; i++)
    {
        e1000_write_command(0x5200 + (i * 4), 0);
    }

    e1000_write_command(0xD0, 0x1F6DC);
    e1000_read_command(0xC0);

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

    /* Liga o Link do Hardware */
    e1000_link_up();

    /* REGISTO POLIMÓRFICO: Acopla a eth0 e o MAC liso no barramento Net */
    net_driver_register(mac_address, &e1000_ops);

    return 0;
}

int e1000_send_package(const void* buffer, uint32_t length)
{
    unsigned long long dest = tx_memory.start + (tx_memory.blocksize * tx_cur);

    if (length > tx_memory.blocksize)
    {
        kprintf("The package is too big\n");
        return 2;
    }

    memcpy((char *)dest, buffer, length);

    tx_descs[tx_cur]->length = length;
    tx_descs[tx_cur]->cmd = CMD_EOP | CMD_IFCS | CMD_RS;
    tx_descs[tx_cur]->status = 0;

    unsigned char old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;

    e1000_write_command(REG_TDT, tx_cur);

    while (!(tx_descs[old_cur]->status & 0xff))
    {
        __asm__ __volatile__("pause;");
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

void module_exit(void)
{
    e1000_hal_stop();
    pool_free((void *)rx_memory.vmem, 98 * 4096);
    pool_free((void *)tx_memory.vmem, 18 * 4096);
    kprintf("[E1000]: Driver cleanup complete.\n");
}