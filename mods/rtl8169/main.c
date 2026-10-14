/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pcnet.c
 *    Description: Módulo Driver Dinâmico (LKM) para a placa de rede Intel rtl8169.
 *                 Suporta Mapeamento MMIO, Alocação Pool DMA, Interrupções MSI
 *                 e integração com a pilha de Rede do Kernel.
 * 
 *          TODO:
 *                  Este driver em parte foi inspirado no driver if_re.c do freeBSD
 *                  E outros codigos de terceiros
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
#include "rtl8169.h"


void re_handle();
int re_send_package(const void* buffer, uint32_t length);

/* ESTRUTURA POLIMÓRFICA DO DRIVER DE REDE */
typedef struct 
{
    int (*transmit)(const void* buffer, uint32_t length);
    int (*receive)(const void* buffer, uint32_t length); 
    int (*open)(void);
    int (*stop)(void);
} net_device_ops_t;


static int re_hal_transmit(const void* buffer, uint32_t length) {
    return re_send_package(buffer, length);
}

static int re_hal_receive(const void* buffer, uint32_t length) {
    (void)buffer;
    (void)length;
    return 0;
}

static int re_hal_open(void) { return 0; }
static int re_hal_stop(void) { return 0; }

static net_device_ops_t re_ops = {
    .transmit = re_hal_transmit,
    .receive  = re_hal_receive,
    .open     = re_hal_open,
    .stop     = re_hal_stop
};

unsigned long re_base_addr;
struct re_softc re_soft;

struct re_type {
        unsigned short  vid;
        unsigned short  did;
        const char      *name;
};

static struct re_type devs[6] = {
    {0,0, "Unknown"},
    {RE_VENDORID, 0x8169, "Realtek PCI GbE Family Controller" },
    {RE_VENDORID, 0x8168, "Realtek PCIe GbE Family Controller" },
    {RE_VENDORID, 0x8161, "Realtek PCIe GbE Family Controller" },
    {RE_VENDORID, 0x8136, "Realtek PCIe FE Family Controller" },
    {DLINK_VENDORID  , 0x4300, "Realtek PCI GbE Family Controller" },
};

struct re_chip_info {
	const char *name;
	unsigned char version;
	unsigned int rx_cfg_mask;
};

//static 
struct re_chip_info re_chip_info[] = {
    {0,                   0,    0x0},
    {0,                   1,    0x0},
    {0,                   2,    0x0},  
    {"RTL-81XX",    MACFG_3,    0xFF00},
    {"RTL-81XX",    MACFG_4,    0xFF00},
    {"RTL-81XX",    MACFG_5,    0xFF00},
    {"RTL-81XX",    MACFG_6,    0xFF00},
    {"RTL-81XX",    MACFG_11,   0xE700},
    {"RTL-81XX",    MACFG_12,   0xE700},
    {"RTL-81XX",    MACFG_13,   0xE700},
    {"RTL-81XX",    MACFG_14,   0xE700},
    {"RTL-81XX",    MACFG_15,   0xE700},
    {"RTL-81XX",    MACFG_16,   0xE700},
    {"RTL-81XX",    MACFG_17,   0xE700},
    {"RTL-81XX",    MACFG_18,   0xE700},
    {"RTL-81XX",    MACFG_19,   0xE700},
    {"RTL-81XX",    MACFG_21,   0xE700},
    {"RTL-81XX",    MACFG_22,   0xE700},
    {"RTL-81XX",    MACFG_23,   0xE700},
    {"RTL-81XX",    MACFG_24,   0xC700},
    {"RTL-81XX",    MACFG_25,   0xC700},
    {"RTL-81XX",    MACFG_26,   0xC700},
    {"RTL-81XX",    MACFG_27,   0xC700},
    {"RTL-81XX",    MACFG_28,   0xC700},
    {"RTL-81XX",    MACFG_31,   0x8700},
    {"RTL-81XX",    MACFG_32,   0x8700},
    {"RTL-81XX",    MACFG_33,   0x8700},
    {"RTL-81XX",    MACFG_36,   0x8700},
    {"RTL-81XX",    MACFG_37,   0x8700},
    {"RTL-81XX",    MACFG_38,   0xBF00},
    {"RTL-81XX",    MACFG_39,   0xBF00},
    {"RTL-81XX",    MACFG_41,   0xE700},
    {"RTL-81XX",    MACFG_42,   0xE700},
    {"RTL-81XX",    MACFG_43,   0xE700},
    {"RTL-81XX",    MACFG_50,   0xBF00},
    {"RTL-81XX",    MACFG_51,   0xBF00},
    {"RTL-81XX",    MACFG_52,   0xBF00},
    {"RTL-81XX",    MACFG_53,   0xE700},
    {"RTL-81XX",    MACFG_54,   0xE700},
    {"RTL-81XX",    MACFG_55,   0xE700},
    {"RTL-81XX",    MACFG_56,   0xCF00},
    {"RTL-81XX",    MACFG_57,   0xCF00},
    {"RTL-81XX",    MACFG_58,   0xCF00},
    {"RTL-81XX",    MACFG_59,   0xCF00},
    {"RTL-81XX",    MACFG_60,   0xCF00},
    {"RTL-81XX",    MACFG_61,   0xCF00},
    {"RTL-81XX",    MACFG_62,   0xCF00},
    {"RTL-81XX",    MACFG_63,   0x8700},
    {"RTL-81XX",    MACFG_64,   0x8700},
    {"RTL-81XX",    MACFG_65,   0x8700},
    {"RTL-81XX",    MACFG_66,   0x8700},
    {"RTL-81XX",    MACFG_67,   0xCF00},
    {"RTL-81XX",    MACFG_68,   0xCF00},
    {"RTL-81XX",    MACFG_69,   0xCF00},
    {"RTL-81XX",    MACFG_FF,   0x0},
};


unsigned int re_read_command(unsigned short addr){
    return *( (volatile unsigned int *)(re_base_addr + addr));
}

void re_write_command(unsigned short addr,unsigned int val){
    *( (volatile unsigned int *)(re_base_addr + addr)) = val;
}

unsigned short re_read_command_word(unsigned short addr){
    return *( (volatile unsigned short *)(re_base_addr + addr));
}

void re_write_command_word(unsigned short addr,unsigned short val){
    *( (volatile unsigned short *)(re_base_addr + addr)) = val;
}

unsigned char re_read_command_byte(unsigned short addr){
    return *( (volatile unsigned char *)(re_base_addr + addr));
}

void re_write_command_byte(unsigned short addr,unsigned char val){
    *( (volatile unsigned char *)(re_base_addr + addr)) = val;
}


int re_probe(struct re_softc * sc, pci_device_t *dev){
    int type = 0;

    for(int i=0; i < 6; i++ ){
        if(devs[i].vid == dev->vendor_id && devs[i].did == dev->device_id ) {
            type = i;
            break;
        }
    }


    sc->vid = devs[type].vid;
    sc->did = devs[type].did;
    sc->type= type;
    kprintf("[RTL81] Vendor ID: %x, Device ID: %x Name: %s\n", devs[type].vid, devs[type].did, devs[type].name);
    return type;
}

static int re_check_mac_version(struct re_softc *sc)
{
        int error = 0;

        switch(re_read_command(0x40) & 0xFCF00000) {
        case 0x00800000:
        case 0x04000000:
                sc->chipset = MACFG_3;
                break;
        case 0x10000000:
                sc->chipset = MACFG_4;
                break;
        case 0x18000000:
                sc->chipset = MACFG_5;
                break;
        case 0x98000000:
                sc->chipset = MACFG_6;
                break;
        case 0x34000000:
        case 0xB4000000:
                sc->chipset = MACFG_11;
                break;
        case 0x34200000:
        case 0xB4200000:
                sc->chipset = MACFG_12;
                break;
        case 0x34300000:
        case 0xB4300000:
                sc->chipset = MACFG_13;
                break;
        case 0x34900000:
        case 0x24900000:
                sc->chipset = MACFG_14;
                break;
        case 0x34A00000:
        case 0x24A00000:
                sc->chipset = MACFG_15;
                break;
        case 0x34B00000:
        case 0x24B00000:
                sc->chipset = MACFG_16;
                break;
        case 0x34C00000:
        case 0x24C00000:
                sc->chipset = MACFG_17;
                break;
        case 0x34D00000:
        case 0x24D00000:
                sc->chipset = MACFG_18;
                break;
        case 0x34E00000:
        case 0x24E00000:
                sc->chipset = MACFG_19;
                break;
        case 0x30000000:
                sc->chipset = MACFG_21;
                break;
        case 0x38000000:
                sc->chipset = MACFG_22;
                break;
        case 0x38500000:
        case 0xB8500000:
        case 0x38700000:
        case 0xB8700000:
                sc->chipset = MACFG_23;
                break;
        case 0x3C000000:
                sc->chipset = MACFG_24;
                break;
        case 0x3C200000:
                sc->chipset = MACFG_25;
                break;
        case 0x3C400000:
                sc->chipset = MACFG_26;
                break;
        case 0x3C900000:
                sc->chipset = MACFG_27;
                break;
        case 0x3CB00000:
                sc->chipset = MACFG_28;
                break;
        case 0x28100000:
                sc->chipset = MACFG_31;
                break;
        case 0x28200000:
                sc->chipset = MACFG_32;
                break;
        case 0x28300000:
                sc->chipset = MACFG_33;
                break;
        case 0x2C100000:
                sc->chipset = MACFG_36;
                break;
        case 0x2C200000:
                sc->chipset = MACFG_37;
                break;
        case 0x2C800000:
                sc->chipset = MACFG_38;
                break;
        case 0x2C900000:
                sc->chipset = MACFG_39;
                break;
        case 0x24000000:
                sc->chipset = MACFG_41;
                break;
        case 0x40900000:
                sc->chipset = MACFG_42;
                break;
        case 0x40A00000:
        case 0x40B00000:
        case 0x40C00000:
                sc->chipset = MACFG_43;
                break;
        case 0x48000000:
                sc->chipset = MACFG_50;
                break;
        case 0x48100000:
                sc->chipset = MACFG_51;
                break;
        case 0x48800000:
                sc->chipset = MACFG_52;
                break;
        case 0x44000000:
                sc->chipset = MACFG_53;
                break;
        case 0x44800000:
                sc->chipset = MACFG_54;
                break;
        case 0x44900000:
                sc->chipset = MACFG_55;
                break;
        case 0x4C000000:
                sc->chipset = MACFG_56;
                break;
        case 0x4C100000:
                sc->chipset = MACFG_57;
                break;
        case 0x50800000:
                sc->chipset = MACFG_58;
                break;
        case 0x50900000:
                sc->chipset = MACFG_59;
                break;
        case 0x5C800000:
                sc->chipset = MACFG_60;
                break;
        case 0x50000000:
                sc->chipset = MACFG_61;
                break;
        case 0x50100000:
                sc->chipset = MACFG_62;
                break;
        case 0x50200000:
                sc->chipset = MACFG_67;
                break;
        case 0x28800000:
                sc->chipset = MACFG_63;
                break;
        case 0x28900000:
                sc->chipset = MACFG_64;
                break;
        case 0x28A00000:
                sc->chipset = MACFG_65;
                break;
        case 0x28B00000:
                sc->chipset = MACFG_66;
                break;
        case 0x54000000:
                sc->chipset = MACFG_68;
                break;
        case 0x54100000:
                sc->chipset = MACFG_69;
                break;
        default:
                kprintf("unknown device\n");
                sc->chipset = MACFG_FF;
                error = -1;
                break;
        }

        return error;
}

static void re_lock(struct re_softc * sc){
    (void)sc;
    re_write_command_byte(0x50, 0x00);
}   

static void re_unlock(struct re_softc * sc){
    (void)sc;
    re_write_command_byte(0x50, 0xC0 );
}


static void re_setup_dma_map_buf(struct re_softc *sc){
    
    uintptr_t addr = (uintptr_t)pool_alloc(0x800000); // Alocar 8MiB

    if(!addr) {
        kernel_panic("[rtl81] realtack");
    }
    memset((void*)addr, 0, 0x800000);

    sc->phymem = vmm_get_physical(addr) & 0x7FFFFFFFFFFFLL;
    sc->vmem = addr;

    sc->tx_buf = sc->phymem;
    sc->rx_buf = sc->phymem + 8192 + (RE_TX_BUF_NUM*8192);

    sc->tx_buf_v = sc->vmem;
    sc->rx_buf_v = sc->vmem + 8192 + (RE_TX_BUF_NUM*8192);
    
    sc->tx_buf_v += 8192;
    sc->rx_buf_v += 8192;

    sc->tx_buf_num = RE_TX_BUF_NUM;
    sc->rx_buf_num = RE_RX_BUF_NUM;

    sc->tx_cur = 0;
    sc->rx_cur = 0;

    for(int i=0; i < RE_TX_BUF_NUM; i++){
        sc->tx_desc[i] = (struct TxDesc *)(sc->vmem + i*sizeof(struct TxDesc));
        unsigned long addr = sc->tx_buf + 8192 + i*8192;
        sc->tx_desc[i]->TxBuffL = addr;
        sc->tx_desc[i]->TxBuffH = addr >> 32;
    }

    for(int i=0; i < RE_RX_BUF_NUM; i++){
        sc->rx_desc[i] = (struct RxDesc *)(sc->vmem + (8192 + RE_TX_BUF_NUM*8192) + i*sizeof(struct RxDesc));
        unsigned long addr = sc->rx_buf + 8192 + i*8192;
        sc->rx_desc[i]->RxBuffL = addr;
        sc->rx_desc[i]->RxBuffH = addr >> 32;
    }
}

static void re_setup_tx_desc(struct re_softc *sc){

    re_write_command(0x20, RL_ADDR_LO(sc->tx_buf));
    re_write_command(0x24, RL_ADDR_HI(sc->tx_buf));
}

static void re_setup_rx_desc(struct re_softc *sc){

    for(int i=0; i < RE_RX_BUF_NUM; i++){

        if(i == (RE_RX_BUF_NUM - 1)){
            sc->rx_desc[i]->Frame_Length = 8192;
            sc->rx_desc[i]->EOR = 1;
            sc->rx_desc[i]->OWN = 1;

        } else{
            sc->rx_desc[i]->Frame_Length = 8192;
            sc->rx_desc[i]->OWN = 1;
         }
    }

    re_write_command(0xe4, RL_ADDR_LO(sc->rx_buf));
    re_write_command(0xe8, RL_ADDR_HI(sc->rx_buf));

}

static void re_get_hw_mac_address(struct re_softc *sc, unsigned char *eaddr){
    (void)sc;
    unsigned int data = re_read_command(0x0000);
    eaddr[0] = ((data & 0x000000FF)>>0) & 0xFF;
    eaddr[1] = ((data & 0x0000FF00)>>8) & 0xFF;
    eaddr[2] = ((data & 0x00FF0000)>>16) & 0xFF;
    eaddr[3] = ((data & 0xFF000000)>>24) & 0xFF;
    data = re_read_command(0x0000 + 4);
    eaddr[4] = ((data & 0x000000FF)>>0) & 0xFF;
    eaddr[5] = ((data & 0x0000FF00)>>8) & 0xFF;
}

int setup_realtek(pci_device_t *dev){
        
    unsigned char mac_address[6];
    memset(&re_soft,0, sizeof(struct re_softc));
    struct re_softc *sc = (struct re_softc *)&re_soft;

    int type = re_probe(sc, dev);
    if(!type) {
        return -1;
    }

    unsigned long up_addr = dev->bar[3];
    re_base_addr = (up_addr << 32 ) & 0xFFFFFFFF00000000;
    re_base_addr |=  dev->bar[2] & 0xFFFFFFF0;
    int intr = dev->irq_line;
    
   
    kprintf("[rtl81] Base Address %x\n", re_base_addr);

    re_soft.type = type;
    re_soft.intr = intr;
    re_soft.base_address = re_base_addr;

    pci_enable_mmio_busmastering(dev);
    kapi_register_irq_handler(dev, re_handle);

    // Mapear para virtual o endereço fisico
    re_base_addr = (unsigned long)vmm_map_device(re_base_addr, 0x400000);
    if (!re_base_addr)
    {
        kernel_panic ("[rtl81] Error: MMIO mapping failed\n");
        return -1;
    }

    // Soft reset the chip
    re_write_command_byte(0x37, RE_CMD_RESET);
    while(re_read_command_byte(0x37) & RE_CMD_RESET){ /*udelay(10);*/}


    // Identify chip attached to board 
    if(re_check_mac_version(sc) ){
        sc->chipset = MACFG_69;
    }

    // get mac address
    re_get_hw_mac_address(&re_soft, mac_address);

    // memory alloc
    re_setup_dma_map_buf(&re_soft);

    re_unlock(&re_soft);

    type = 0;
    // RTL-8169sb/8110sb or previous version
    if (sc->chipset <= 5 ){
        // Enable transmit and receive
        // RE_COMMAND
        re_write_command_byte(0x37, RE_CMD_TX_ENB | RE_CMD_RX_ENB);

    }

    // EarlyTxThres
    // TxMaxSize Jumbo
    re_write_command_byte(0xec, 0x40 -1);
    // RxMaxSize
    re_write_command_word(0xda, 8192 -1);

    // Set Rx Config register
    // RE_RXCFG
    unsigned int cfg = 7 << 13 | 6 << 8 | 0xf;
    cfg |= (re_read_command(0x44) & 0xff7e1880);
    //cfg = re_chip_info[sc->chipset].rx_cfg_mask | 0xF;
    re_write_command(0x44, cfg);

    // Set the initial TX configuration
    // Set DMA burst size and Interframe Gap Time 
    // RE_TXCFG
    cfg = 3 << 24 | 6 << 8;
    re_write_command(0x40, cfg); 

    re_setup_tx_desc(&re_soft);
    re_setup_rx_desc(&re_soft);


    // RTL-8169sc/8110sc or later version
    if (sc->chipset > 5){
        // Enable transmit and receive
        // RE_COMMAND
        re_write_command_byte(0x37, RE_CMD_TX_ENB | RE_CMD_RX_ENB);

    }

    // RxMissed
    re_write_command(0x4c, 0);
    
    // no early-rx interrupts
    re_write_command_word(0x5c, re_read_command_word(0x5c) & 0xF000);

    // Timerint
    re_write_command(0x58, 0);
    // Enable interrupts.
    unsigned int re_intrs = 0x8000 | 0x4000 | 0x2000 | 0x0040 | 0x0020 | 0x0010 | 0x0008 | 0x0002 | 0x0001;
    re_write_command_word(0x3c, re_intrs);


    re_lock(&re_soft);

    net_driver_register(mac_address, &re_ops);
    
    net_set_link_event(NET_EVENT_LINK_UP);

    return 0;
}


int re_send_package(const void* buffer, uint32_t length){


    struct re_softc *sc = (struct re_softc *)&re_soft;

    unsigned long long dest = sc->tx_buf_v + (8192*sc->tx_cur);

    memcpy((char*)dest, buffer, length);

    sc->tx_desc[sc->tx_cur]->Frame_Length = length;
    sc->tx_desc[sc->tx_cur]->OWN = 1;
    sc->tx_desc[sc->tx_cur]->EOR = 1;
    sc->tx_desc[sc->tx_cur]->FS = 1;
    sc->tx_desc[sc->tx_cur]->LS = 1;
    sc->tx_desc[sc->tx_cur]->IPCS = 1;

    /* TXPoll
	HPQ		= 0x80		// Poll cmd on the high prio queue
	NPQ		= 0x40		// Poll cmd on the low prio queue 
	FSWInt	= 0x01		// Forced software interrupt 
    */

    re_write_command_byte(0x38, 0x40); // set polling bit
    while(re_read_command_byte(0x38) &0x40 ){ __asm__ __volatile__("pause;");}

    //sc->tx_cur = (sc->tx_cur + 1) % sc->tx_buf_num;

    return 0;
}

void re_recieve_package(){

    struct re_softc *sc = (struct re_softc *)&re_soft;
    
    while(sc->rx_desc[sc->rx_cur]->OWN == 0){

        sc->rx_desc[sc->rx_cur]->OWN = 1;

        unsigned long long addr = sc->rx_buf_v + (8192*sc->rx_cur);

        unsigned short len = sc->rx_desc[sc->rx_cur]->Frame_Length;
        sc->rx_cur = (sc->rx_cur + 1) % sc->rx_buf_num;

        net_driver_receive((const void *)addr, (uint32_t)len);
    }      
    
}

void re_handle(){
    
    unsigned short status = re_read_command_word(0x3e);
    //kprintf("[RTL81] Interrupt detected %x\n", status );
    if (status == 0) return;

    if(status&0x20) {

	unsigned char media_status = re_read_command_byte(RE_MEDIASTATUS);
        
        if (!(media_status & MEDIA_LINK_OK)) 
        {
            net_set_link_event(NET_EVENT_LINK_UP);
        } 
        else 
        {
            net_set_link_event(NET_EVENT_LINK_DOWN);
        }
    }

    if(status&0x01) {
	//kprintf("[RTL81] Package recieved!\n");
        re_recieve_package();

    }
    
    if(status&0x04) {

	 //kprintf("[RTL81] Package send!\n");
    }

    re_write_command_word(0x3e, status);
}

int module_init(pci_device_t *dev)
{
    setup_realtek(dev);
    return 0;
}

void module_exit(void)
{
    kprintf("[rtl1869]: Driver cleanup complete.\n");
    pool_free((void*)re_soft.vmem, 0x800000);
}