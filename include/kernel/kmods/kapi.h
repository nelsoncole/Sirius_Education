/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kapi.h
 *    Description: Interface de Programação do Kernel (Kernel API) para Módulos.
 *                 Agrupa os protótipos de funções exportadas pelo núcleo
 *                 para o desenvolvimento seguro de drivers dinâmicos (LKM).
 * 
 *         Author: Nelson Cole
 *   Created Date: 19/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 19/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _KAPI_H_
#define _KAPI_H_

#include <kernel/lib/stddef.h>
#include <kernel/lib/stdint.h>

/* ============================================================================
 * 1. GESTÃO BASE, DEPURAÇÃO E ALOCAÇÃO DE HEAP
 * ============================================================================
 */
extern int  kprintf(const char *fmt, ...);
extern void *kmalloc(size_t size);
extern void kfree(void *ptr);

/* ============================================================================
 * 2. BIBLIOTECA C INTERNA (Obrigatórias por otimização de código do GCC)
 * ============================================================================
 */
extern size_t strlen(const char *str);
extern int    strcmp(const char *s1, const char *s2);
extern void   *memset(void *s, int c, size_t n);
extern void   *memcpy(void *dst, const void *src, size_t n);

/* ============================================================================
 * 3. GESTÃO DE MEMÓRIA FÍSICA (PMM - Alocação e Libertação de Páginas/Frames)
 * ============================================================================
 */
extern unsigned long pmm_alloc_page(void);
extern void          pmm_free_page(unsigned long phys_address);
extern unsigned long pmm_alloc_pages(unsigned long count);
extern void          pmm_free_pages(unsigned long phys_address, unsigned long count);

/* ============================================================================
 * 4. GESTÃO DE MEMÓRIA VIRTUAL (VMM - Mapeamento de Dispositivos e MMIO)
 * ============================================================================
 */
extern void *vmm_map_device(unsigned long phys_addr, unsigned long size);
extern void *vmm_scratch_map(unsigned long phys_addr);
extern uintptr_t vmm_get_physical(uintptr_t virtual_address);

/* ============================================================================
 * 5. ALOCAÇÃO SÍNCRONA NA POOL DMA (Memória Contígua para Buffers de Hardware)
 * ============================================================================
 */
extern void *pool_alloc(size_t size);
extern void pool_free(void *virt_addr, size_t size);

/* ============================================================================
 * 6. CONTROLO DE INTERRUPÇÕES AVANÇADAS (Kernel API com suporte a MSI)
 * ============================================================================
 */
extern void kapi_register_irq_handler(void *dev, void (*fuc)(void));

/* ============================================================================
 * 7. SUBSISTEMA DE FICHEIROS VIRTUAL (VFS - Registo de File Systems)
 * ============================================================================
 */
extern int vfs_register_filesystem(void *fs);

/* ============================================================================
 * 8. SUBSISTEMA DE REDE CENTRAL (Pilha Network / HAL Multi-Interface)
 * ============================================================================
 */
extern int net_driver_register(const uint8_t *mac_addr, void *ops_table);
extern int net_driver_receive(const void *buffer, uint32_t packet_size);

/* ============================================================================
 * 9. SUBSISTEMA DE CONTROLO DO BARRAMENTO PCI
 * ============================================================================
 */
extern uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
extern void     pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t data);
extern void     pci_enable_mmio_busmastering(void *dev);
extern int pci_load_devices_by_class(uint8_t class_code, uint8_t subclass_code, int (*init_cb)(void *dev));

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

#endif /* _KAPI_H_ */
