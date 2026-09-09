/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ahci.h
 *    Description: Definições de estruturas e macros para o protocolo AHCI.
 *                 Contém os descritores de paginação de tabelas, cabeçalhos 
 *                 de comandos e registos FIS para transferências DMA nativas.
 * 
 *         Author: Nelson Cole
 *   Created Date: 08/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 08/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _AHCI_H_
#define _AHCI_H_

#include <kernel/lib/stdint.h>
#include <kernel/drivers/bus/pci.h>

#define PCI_CLASS_STORAGE       0x01
#define PCI_SUBCLASS_SATA       0x06
#define PCI_PROGIF_AHCI         0x01

#define AHCI_SATA_SIG_ATA       0x00000101  // Disco Rígido / SSD SATA
#define AHCI_SATA_SIG_ATAPI     0xEB140101  // Drive de CD/DVD
#define AHCI_SATA_SIG_SEMB      0xC33C0101  // Enclosure Management Bridge
#define AHCI_SATA_SIG_PM        0x96690101  // Port Multiplier

#define HBA_PORT_DET_PRESENT    3
#define HBA_PORT_IPM_ACTIVE     1

#define FIS_TYPE_REG_H2D        0x27

/* Bits do Registo de Controlo Global do Host (GHC) */
#define AHCI_GHC_HR             (1 << 0)    // HBA Reset (Reset por software)
#define AHCI_GHC_IE             (1 << 1)    // Interrupt Enable (Habilita Interrupções Globais)
#define AHCI_GHC_AE             (1 << 31)   // AHCI Enable (Ativa o modo AHCI nativo)

/* Bits do Registo de Comando da Porta (CMD) */
#define AHCI_PxCMD_ST           (1 << 0)    // Start (Ativa o processamento da lista de comandos)
#define AHCI_PxCMD_SUD          (1 << 1)    // Spin-Up Device (Acorda o disco se necessário)
#define AHCI_PxCMD_POD          (1 << 2)    // Power On Device
#define AHCI_PxCMD_FRE          (1 << 4)    // FIS Receive Enable (Permite receber estruturas FIS)
#define AHCI_PxCMD_FR           (1 << 14)   // FIS Receive Running (Indica se a receção de FIS está ativa)
#define AHCI_PxCMD_CR           (1 << 15)   // Command List Running (Indica se a lista de comandos está ativa)

/**
 * Estrutura do Frame Information Structure (FIS) de Registo Host-to-Device.
 */
typedef struct {
    uint8_t  fis_type;  // Tipo de FIS (FIS_TYPE_REG_H2D = 0x27)
    uint8_t  pmport:4;  // Port Multiplier Port
    uint8_t  rsv0:3;    // Reservado
    uint8_t  c:1;       // Command/Control (1 = Comando, 0 = Controlo)
    uint8_t  command;   // Código do comando ATA (ex: ATA_CMD_READ_DMA_EXT)
    uint8_t  featurel;  // Funcionalidade (Bits inferiores)
    uint8_t  lba0;      // LBA byte 0
    uint8_t  lba1;      // LBA byte 1
    uint8_t  lba2;      // LBA byte 2
    uint8_t  device;    // Seleção de dispositivo (Bit 6 ativo força modo LBA)
    uint8_t  lba3;      // LBA byte 3
    uint8_t  lba4;      // LBA byte 4
    uint8_t  lba5;      // LBA byte 5
    uint8_t  featureh;  // Funcionalidade (Bits superiores)
    uint8_t  countl;    // Contagem de setores (Bits inferiores)
    uint8_t  counth;    // Contagem de setores (Bits superiores)
    uint8_t  icc;       // Isochronous Command Completion
    uint8_t  control;   // Registo de controlo
    uint8_t  rsv1[4];   // Reservado
} __attribute__((packed)) h2d_register_fis_t;

/**
 * Entrada da Physical Region Descriptor Table (PRDT).
 * Define as regiões físicas de memória RAM envolvidas na transferência DMA.
 */
typedef struct {
    uint32_t dba;       // Endereço físico base dos dados (32 bits inferiores)
    uint32_t dbau;      // Endereço físico base dos dados (32 bits superiores)
    uint32_t rsv0;      // Reservado
    uint32_t dbc:22;    // Data Byte Count (Tamanho em bytes da região - 1)
    uint32_t rsv1:9;    // Reservado
    uint32_t i:1;       // Interrupt on Completion (1 = Ativar interrupção para esta entrada)
} __attribute__((packed)) hba_prdt_entry_t;

/**
 * Estrutura da Command Table (Tabela de Comandos do AHCI).
 * Agrupa o descritor FIS e a tabela PRDT para mapeamento direto de DMA Scatter-Gather.
 */
typedef struct {
    uint8_t  cfis[64];              // Command FIS (Região reservada para armazenamento de FIS)
    uint8_t  acmd[16];              // ATAPI Command (Pacote de comandos de 12 ou 16 bytes para CD/DVD)
    uint8_t  rsv[48];               // Reservado
    hba_prdt_entry_t prdt_entry;    // Entrada PRDT única de alto rendimento
} __attribute__((packed)) hba_cmd_tbl_t;

/**
 * Estrutura do Cabeçalho de Comando (Command Header).
 * Cada uma das 32 portas possui uma lista com 32 cabeçalhos destes na memória.
 */
typedef struct {
    uint8_t  cfl:5;     // Command FIS Length (Comprimento em DWORDS do FIS)
    uint8_t  a:1;       // ATAPI (1 = Dispositivo ATAPI, CD-ROM)
    uint8_t  w:1;       // Write (1 = Escrita na unidade, 0 = Leitura da unidade)
    uint8_t  p:1;       // Prefetchable
    uint8_t  r:1;       // Reset
    uint8_t  b:1;       // BIST
    uint8_t  c:1;       // Clear Busy upon R_OK
    uint8_t  rsv0:1;    // Reservado
    uint8_t  pmp:4;     // Port Multiplier Port
    uint16_t prdtl;     // Physical Region Descriptor Table Length (Número de entradas PRDT)
    uint32_t prdbc;     // Physical Region Descriptor Byte Count (Contador de bytes transferidos)
    uint32_t ctba;      // Endereço físico da Command Table Base (32 bits inferiores)
    uint32_t ctbau;     // Endereço físico da Command Table Base (32 bits superiores)
    uint32_t rsv1[4];   // Reservado
} __attribute__((packed)) hba_cmd_header_t;

/* Registos específicos de cada porta AHCI (Tamanho: 128 bytes por porta) */
typedef struct {
    uint32_t clb;       // 0x00, Command List Base Address (32 bits inferiores)
    uint32_t clbu;      // 0x04, Command List Base Address (32 bits superiores)
    uint32_t fb;        // 0x08, FIS Base Address (32 bits inferiores)
    uint32_t fbu;       // 0x0C, FIS Base Address (32 bits superiores)
    uint32_t is;        // 0x10, Interrupt Status
    uint32_t ie;        // 0x14, Interrupt Enable
    uint32_t cmd;       // 0x18, Command and Status
    uint32_t rsv0;      // 0x1C, Reservado
    uint32_t tfd;       // 0x20, Task File Data
    uint32_t sig;       // 0x24, Signature
    uint32_t ssts;      // 0x28, Serial ATA Status (SCR0: SStatus)
    uint32_t sctl;      // 0x2C, Serial ATA Control (SCR1: SControl)
    uint32_t serr;      // 0x30, Serial ATA Error (SCR2: SError)
    uint32_t sact;      // 0x34, Serial ATA Active (SCR3: SActive)
    uint32_t ci;        // 0x38, Command Issue
    uint32_t sntf;      // 0x3C, Serial ATA Notification
    uint32_t fbs;       // 0x40, FIS-based Switching Control
    uint32_t rsv1[11];  // 0x44 ~ 0x6F, Reservado
    uint32_t vendor[4]; // 0x70 ~ 0x7F, Vendor Specific
} __attribute__((packed)) hba_port_t;

/* Estrutura de memória global exposta no BAR5 do AHCI (Generic Host Control) */
typedef struct {
    uint32_t cap;       // 0x00, Host Capabilities
    uint32_t ghc;       // 0x04, Global Host Control
    uint32_t is;        // 0x08, Interrupt Status Register
    uint32_t pi;        // 0x0C, Ports Implemented (Bitmask das portas ativas)
    uint32_t vs;        // 0x10, AHCI Version
    uint32_t ccc_ctl;   // 0x14, Command Completion Coalescing Control
    uint32_t ccc_pts;   // 0x18, Command Completion Coalescing Ports
    uint32_t em_loc;    // 0x1C, Enclosure Management Location
    uint32_t em_ctl;    // 0x20, Enclosure Management Control
    uint32_t cap2;      // 0x24, Host Capabilities Extended
    uint32_t bohc;      // 0x28, BIOS/OS Handoff Control and Status
    uint8_t  rsv[116];  // 0x2C ~ 0x9F, Reservado
    uint8_t  vendor[96];// 0xA0 ~ 0xFF, Vendor Specific
    hba_port_t ports[32]; // 0x100, Até 32 portas físicas implementadas
} __attribute__((packed)) hba_mem_t;

/* Interfaces Públicas Exportadas */
int ahci_init(pci_device_t *dev);
void ahci_driver_init(void);

int ahci_read_blocks(int device_id, uint64_t lba, uint32_t count, uintptr_t phys_buffer);
int ahci_write_blocks(int device_id, uint64_t lba, uint32_t count, uintptr_t phys_buffer);

#endif /* _AHCI_H_ */
