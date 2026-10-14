/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pcnet.c
 *    Description: Módulo Driver Dinâmico (LKM) para a placa de rede Intel pcnet.
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
#include "pcnet.h"

#define PCI_CLASS_NETWORK       0x02
#define PCI_SUBCLASS_ETHERNET   0x00

static unsigned int io_base;
struct pcnet_softc pcnet_softc;
struct pcnet_block *pcnet_block;
/* Estado estático no driver para evitar disparar eventos repetidos a cada IRQ */
static bool g_pcnet_last_link_state = false;

int pcnet_send_package(const void* buffer, uint32_t length);
static void irq_pcnet();

static inline void outw(uint16_t port, uint16_t val)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static unsigned short pcnetReadCSR(unsigned char reg)
{
	outw(io_base + REG_RAP, reg);
	return inw(io_base + REG_RDP);
}
static void pcnetWriteCSR(unsigned char reg, unsigned short val)
{
	outw(io_base + REG_RAP, reg);
	outw(io_base + REG_RDP, val);
}
static unsigned short pcnetReadBCR(unsigned char reg)
{
	outw(io_base + REG_RAP, reg);
	return inw(io_base + REG_BDP);
}
void pcnetWriteBCR(unsigned char reg, unsigned short val)
{
	outw(io_base + REG_RAP, reg);
	outw(io_base + REG_BDP, val);
}

static void pcnet_setup_dma_map_buf(struct pcnet_softc *sc){
    uintptr_t addr = (uintptr_t)pool_alloc(0x80000); // 512 KiB

    if(!addr) {
        kernel_panic("pcnet");
    }
    memset((void*)addr, 0, 0x80000);

    sc->phymem = vmm_get_physical(addr) & 0x7FFFFFFFFFFFLL;
    sc->vmem = addr;

    if(sc->phymem > (0xFFFFFFFF - 0x80000 )) {
        kernel_panic("pcnet, the pcnet fest3 not support 64-bit memory physical address");
    }


    sc->pcnet_block_vmem = sc->vmem;
    sc->pcnet_block_phymem = sc->phymem;

    sc->phymem += 8192;
    sc->vmem += 8192;

    sc->tx_buf = sc->phymem;
    sc->rx_buf = sc->phymem + 8192 + (PCNET_TX_BUF_NUM*8192);

    sc->tx_buf_v = sc->vmem;
    sc->rx_buf_v = sc->vmem + 8192 + (PCNET_TX_BUF_NUM*8192);
    
    sc->tx_buf_v += 8192;
    sc->rx_buf_v += 8192;

    sc->tx_buf_num = PCNET_TX_BUF_NUM;
    sc->rx_buf_num = PCNET_RX_BUF_NUM;

    sc->tx_cur = 0;
    sc->rx_cur = 0;

    for(int i=0; i < PCNET_TX_BUF_NUM; i++){
        sc->tx_desc[i] = (struct TxDesc *)(sc->vmem + i*sizeof(struct TxDesc));
        unsigned long addr = sc->tx_buf + 8192 + i*8192;
        sc->tx_desc[i]->buffer = (unsigned int)addr;
    }

    for(int i=0; i < PCNET_RX_BUF_NUM; i++){
        sc->rx_desc[i] = (struct RxDesc *)(sc->vmem + (8192 + PCNET_TX_BUF_NUM*8192) + i*sizeof(struct RxDesc));
        unsigned long addr = sc->rx_buf + 8192 + i*8192;
        sc->rx_desc[i]->buffer = (unsigned int)addr;
        unsigned short bcnt = (unsigned short)(-8192);
        bcnt &= 0x0fff;
        bcnt |= 0xf000;
        sc->rx_desc[i]->buf_length = bcnt;
        sc->rx_desc[i]->status = 0x8000;
    }
}

static void pcnet_get_hw_mac_address(unsigned char *eaddr){

    unsigned int data = pcnetReadCSR(12);
    data |= pcnetReadCSR(12 + 1) << 16;
    eaddr[0] = ((data & 0x000000FF)>>0) & 0xFF;
    eaddr[1] = ((data & 0x0000FF00)>>8) & 0xFF;
    eaddr[2] = ((data & 0x00FF0000)>>16) & 0xFF;
    eaddr[3] = ((data & 0xFF000000)>>24) & 0xFF;
    data = pcnetReadCSR(12 + 2);
    eaddr[4] = ((data & 0x000000FF)>>0) & 0xFF;
    eaddr[5] = ((data & 0x0000FF00)>>8) & 0xFF;
}

static int pcnet_hal_transmit(const void* buffer, uint32_t length) {
    return pcnet_send_package(buffer, length);
}

static int pcnet_hal_receive(const void* buffer, uint32_t length) {
    (void)buffer;
    (void)length;
    return 0;
}

static int pcnet_hal_open(void) { return 0; }
static int pcnet_hal_stop(void) { return 0; }

/* ESTRUTURA POLIMÓRFICA DO DRIVER DE REDE */
typedef struct 
{
    int (*transmit)(const void* buffer, uint32_t length);
    int (*receive)(const void* buffer, uint32_t length); 
    int (*open)(void);
    int (*stop)(void);
} net_device_ops_t;

static net_device_ops_t pcnet_ops = {
    .transmit = pcnet_hal_transmit,
    .receive  = pcnet_hal_receive,
    .open     = pcnet_hal_open,
    .stop     = pcnet_hal_stop
};

int setup_pcnet( pci_device_t *dev){
    unsigned char mac_address[6];
    memset(&pcnet_softc,0, sizeof(struct pcnet_softc));
    struct pcnet_softc *sc = &pcnet_softc;

    io_base = dev->bar[0];
    if (!io_base)
    {
        return -1;
    }

    io_base &= ~1;

    kprintf("[pcnet] io base address: %x \n",io_base);

    pci_enable_mmio_busmastering(dev);
    kapi_register_irq_handler(dev, irq_pcnet);

    pcnet_setup_dma_map_buf(sc);
    //outportd(io_base + REG_RDP, 0);

    pcnet_get_hw_mac_address(mac_address);

    // Data Structure Initialization Block
    pcnet_block = (struct pcnet_block *)sc->pcnet_block_vmem;
   
	pcnet_block->mode = (3 << 28) | (5 << 20);
	pcnet_block->phys_addr1 = 0;
	memcpy(&pcnet_block->phys_addr0, mac_address, 6);
	pcnet_block->logical_addr_f0 = 0;
	pcnet_block->logical_addr_f1 = 0;
	pcnet_block->rx_buffer = sc->rx_buf;
	pcnet_block->tx_buffer = sc->tx_buf;

    // reset card
	//inportw(io_base + REG_RESET);

    // Switch pcnet to 32bit mode
	pcnetWriteBCR(20, 2);
    
    // Set reset autoselect bit
	int val = pcnetReadBCR(2) & ~2;
	val |= 2;
	pcnetWriteBCR(2, val);

	/* Enable auto negotiate, setup, disable fd */
	val = pcnetReadBCR(32) & ~0x98;
	val |= 0x20;
	pcnetWriteBCR(32, val);


	// Initialise
	unsigned int paddr = sc->pcnet_block_phymem;
	pcnetWriteCSR(1, paddr & 0xffff);
	pcnetWriteCSR(2, (paddr >> 16) & 0xffff);

    pcnetWriteCSR(4, 0x0915);
	pcnetWriteCSR(0, 0x0001);	// start

    // Wait for Init Done bit
    int i;
	for (i = 10000; i > 0; i--) {
		if (pcnetReadCSR(0) & 0x0100)
			break;
		//udelay (10);
	}
	if (i <= 0) {
		kernel_panic("[pcnet]: timeout: controller init failed\n");
        return -1;
	}

    pcnetWriteCSR(0, CSR_STATUS_INIT | CSR_STATUS_IENA | CSR_STATUS_STRT);


    /* REGISTO POLIMÓRFICO: Acopla a eth0 e o MAC liso no barramento Net */
    net_driver_register(mac_address, &pcnet_ops);

    g_pcnet_last_link_state = true;
    kprintf("[pcnet]: Link ativo detetado no boot. Disparando evento...\n");
    net_set_link_event(NET_EVENT_LINK_UP);

    return 0;
}

int pcnet_send_package(const void* buffer, uint32_t length) {

    struct pcnet_softc *sc = (struct pcnet_softc *)&pcnet_softc;
    
    unsigned long long dest = sc->tx_buf_v + (8192*sc->tx_cur);

    memcpy((char*)dest, buffer, length);

    int pkt_len = length;
    sc->tx_desc[sc->tx_cur]->length = (-pkt_len);
    sc->tx_desc[sc->tx_cur]->misc = 0x00000000;
    sc->tx_desc[sc->tx_cur]->status = 0x8300;
    pcnetWriteCSR(0, pcnetReadCSR(0)| 0x0008);

    // next ptr
    sc->tx_cur = (sc->tx_cur + 1) % sc->tx_buf_num;
    return 0;
}

void pcnet_recieve_package(void)
{
    struct pcnet_softc *sc = (struct pcnet_softc *)&pcnet_softc;
   
    while(!(sc->rx_desc[sc->rx_cur]->status & 0x8000) ){

        sc->rx_desc[sc->rx_cur]->status |= 0x8000;

        unsigned long long addr = sc->rx_buf_v + (8192*sc->rx_cur);

        unsigned short len = (sc->rx_desc[sc->rx_cur]->msg_length & 0xfff) - 4;
        sc->rx_cur = (sc->rx_cur + 1) % sc->rx_buf_num;

       net_driver_receive((const void *)addr, (uint32_t)len);
    }
}

static void irq_pcnet(){
    unsigned int status = pcnetReadCSR(0);
    /* Se o bit INTR (bit 7) estiver zerado, esta interrupção não foi gerada pela PCNet */
    if (!(status & 0x0080)) 
    {
        return;
    }
    
    //kprintf("[PCNET] Interrupt detected %x\n", status );

    status &= ~CSR_STATUS_INTR;

    /*unsigned int bcr4 = pcnetReadBCR(PCNET_BCR4); 
    bool current_link_up = (bcr4 & PCNET_BCR4_LNKST) ? true : false;

    kprintf("%x %d\n", bcr4, current_link_up);
    if (current_link_up != g_pcnet_last_link_state)
    {
        g_pcnet_last_link_state = current_link_up;
        
        if (current_link_up)
        {
            net_set_link_event(NET_EVENT_LINK_UP);
        }
        else
        {
            net_set_link_event(NET_EVENT_LINK_DOWN);
        }
    }*/

    if( status & CSR_STATUS_RINT )
	{
		//kprintf("[PCNET] Package recieved!\n");
        pcnet_recieve_package(); 
	}
	
	// Tx Interrupt
	if( status & CSR_STATUS_TINT )
	{
		//kprintf("[PCNET] Package send!\n");
	}

    if( status & CSR_STATUS_IDON )
	{
		//kprintf("CSR15 reads as 0x%x\n", pcnetReadCSR(15));
	}

    // set interrupt as handled
    pcnetWriteCSR(0, status);
    // don't forget to send EOI
}

int module_init(pci_device_t *dev)
{
    setup_pcnet(dev);
    return 0;
}

void module_exit(void)
{
    pool_free((void*)pcnet_softc.vmem, 0x80000);
    kprintf("[pcnet]: Driver cleanup complete.\n");
}