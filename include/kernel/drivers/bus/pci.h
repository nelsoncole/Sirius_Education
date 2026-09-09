/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pci.h
 *    Description: Cabeçalho do subsistema de barramento PCI (Peripheral Component
 *                 Interconnect). Define as portas de E/S, deslocamentos do
 *                 Espaço de Configuração e os códigos de classe de hardware.
 * 
 *         Author: Nelson Cole
 *   Created Date: 07/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 07/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _PCI_H_
#define _PCI_H_

#include <kernel/lib/stdint.h>


#define PCI_CONFIG_ADDRESS  0x0CF8  /* Porta de seleção do endereço PCI */
#define PCI_CONFIG_DATA     0x0CFC  /* Porta de leitura/escrita de dados */

#define PCI_REG_VENDOR_ID   0x00    /* Identificador do Fabricante (2 Bytes) */
#define PCI_REG_DEVICE_ID   0x02    /* Identificador do Dispositivo (2 Bytes) */
#define PCI_REG_COMMAND     0x04    /* Registo de Comando de Hardware (2 Bytes) */
#define PCI_REG_STATUS      0x06    /* Registo de Estado do Dispositivo (2 Bytes) */
#define PCI_REG_REVISION    0x08    /* Revisão do Silício (1 Byte) */
#define PCI_REG_PROG_IF     0x09    /* Interface de Programação - Programming Interface (1 Byte) */
#define PCI_REG_SUBCLASS    0x0A    /* Subclasse do Dispositivo (1 Byte) */
#define PCI_REG_CLASS       0x0B    /* Classe Principal do Dispositivo (1 Byte) */
#define PCI_REG_CACHE_LINE  0x0C    /* Cache Line Size (1 Byte) */
#define PCI_REG_LATENCY     0x0D    /* Latency Timer (1 Byte) */
#define PCI_REG_HEADER_TYPE 0x0E    /* Tipo de Cabeçalho / Função Múltipla (1 Byte) */
#define PCI_REG_BIST        0x0F    /* Built-In Self Test (1 Byte) */

/* Registos de Endereço de Base (Base Address Registers) */
#define PCI_REG_BAR0        0x10    /* BAR 0 (4 ou 8 Bytes) */
#define PCI_REG_BAR1        0x14    /* BAR 1 (4 Bytes) */
#define PCI_REG_BAR2        0x18    /* BAR 2 (4 Bytes) */
#define PCI_REG_BAR3        0x1C    /* BAR 3 (4 Bytes) */
#define PCI_REG_BAR4        0x20    /* BAR 4 (4 Bytes) */
#define PCI_REG_BAR5        0x24    /* BAR 5 (4 Bytes) */

/* Linhas de Interrupção */
#define PCI_REG_INTERRUPT_LINE  0x3C /* Linha do IRQ encaminhado (1 Byte) */
#define PCI_REG_INTERRUPT_PIN   0x3D /* Pino INT# físico (A, B, C, D) (1 Byte) */

/* Codigos de classe de Hardware relevantes */
#define PCI_CLASS_UNCLASSIFIED  0x00
#define PCI_CLASS_STORAGE       0x01  /* Controladores de Disco (IDE, SATA/AHCI, NVMe) */
#define PCI_CLASS_NETWORK       0x02  /* Placas de Rede (Ethernet, Wi-Fi) */
#define PCI_CLASS_DISPLAY       0x03  /* Placas Gráficas (VGA, GOP/UEFI emuladores) */
#define PCI_CLASS_MULTIMEDIA    0x04  /* Dispositivos de Áudio/Vídeo */
#define PCI_CLASS_BRIDGE        0x06  /* Pontes ISA, LPC, Host-to-PCI */
#define PCI_CLASS_SERIAL_BUS    0x0C  /* Controladores USB (UHCI, EHCI, xHCI), FireWire */

/* Subclasses úteis de Armazenamento */
#define PCI_SUBCLASS_IDE        0x01
#define PCI_SUBCLASS_SATA       0x06  /* Interface AHCI enquadra-se aqui */

/* Estrutura que representa a localização física de uma função no barramento */
typedef struct {
    uint8_t bus;        /* Intervalo: 0 a 255 */
    uint8_t device;     /* Intervalo: 0 a 31 */
    uint8_t function;   /* Intervalo: 0 a 7 (Dispositivos Multi-Função) */
} pci_address_t;

/* Bloco de dados de um dispositivo detetado e mapeado pelo Kernel */
typedef struct pci_device {
    pci_address_t address;      /* Coordenadas lógicas do barramento */
    uint16_t vendor_id;         /* ID do fabricante */
    uint16_t device_id;         /* ID do produto */
    uint8_t  class_code;        /* Classe principal (ex: 0x02 para Rede) */
    uint8_t  subclass_code;     /* Subclasse (ex: 0x00 para Ethernet) */
    uint8_t  prog_if;           /* Interface de programação */
    uint32_t bar[6];            /* Mapeamento das bases físicas de E/S ou MMIO */
    uint8_t  irq_line;          /* Vetor de interrupção atribuído */
    
    struct pci_device* next;    /* Ponteiro para encadeamento de lista no Kernel */
} pci_device_t;


/**
 * Inicializa e varre recursivamente todos os barramentos PCI do sistema.
 * Identifica, mapeia e encadeia os dispositivos localizados.
 */
void pci_bus_init(void);

/**
 * Efetua a leitura de um dado de 32-bits (Dword) do espaço de configuração PCI.
 */
uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

/**
 * Escreve um dado de 32-bits (Dword) no espaço de configuração PCI.
 */
void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t data);

/**
 * Interface pública de registo para drivers dinâmicos (LKMs) ou embutidos.
 * 
 * @param name      Nome legível do driver.
 * @param vendor_id Fabricante alvo.
 * @param device_id Dispositivo alvo.
 * @param probe_cb  Função callback invocada se o hardware coincidir na varredura.
 * @return 0 em caso de sucesso, ou erro negativo se falhar.
 */
int pci_register_driver(const char* name, uint16_t vendor_id, uint16_t device_id, int (*probe_cb)(pci_device_t* dev));


/**
 * Procura um dispositivo através dos códigos de Classe e Subclasse.
 * Essencial para o design moderno do Sirius_Education, permitindo detetar o 
 * controlador I2C (Classe 0x0C, Subclasse 0x80) de forma genérica.
 * 
 * @param from Dispositivo a partir do qual continuar a busca (use NULL para iniciar do topo).
 * @return Ponteiro para o próximo dispositivo correspondente encontrado, ou NULL.
 */
pci_device_t* pci_find_by_class(uint8_t class_code, uint8_t subclass_code, pci_device_t *from);

/**
 * Expõe a cabeça da lista ligada de dispositivos PCI.
 * Permite que loops externos enumerem recursivamente todo o hardware mapeado.
 */
pci_device_t* pci_get_devices_head(void);

/**
 * Procura e carrega em loop todos os dispositivos que pertençam a uma mesma 
 * classe e subclasse. Resolve o cenário de máquinas com múltiplos controladores
 * USB (ex: xHCI e eHCI) ou múltiplos barramentos I2C.
 * 
 * @param class_code Código da classe PCI procurada.
 * @param subclass_code Código da subclasse PCI procurada.
 * @param init_cb Função callback que inicializa o driver específico deste hardware.
 * @return O número total de dispositivos carregados com sucesso.
 */
int pci_load_devices_by_class(uint8_t class_code, uint8_t subclass_code, int (*init_cb)(pci_device_t *dev));

/**
 * Ativa os recursos fundamentais de barramento para um dispositivo PCI mapeado:
 * - Bus Mastering (Permite transações DMA nativas de alta velocidade)
 * - Memory Space MMIO (Habilita a resposta aos BARs de memória)
 */
void pci_enable_mmio_busmastering(pci_device_t *dev);

#endif
