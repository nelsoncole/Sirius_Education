/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: symbols.c
 *    Description: Tabela de exportação de símbolos globais do Kernel e 
 *                 mecanismo de busca para ligação de módulos dinâmicos.
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

#include <kernel/kmods/kmod.h>

/* Declaração das funções do núcleo do Kernel que serão exportadas */
extern int kprintf2(const char *fmt, ...);
extern void *kmalloc(size_t size);
extern void kfree(void *ptr);

/* Biblioteca C Interna (Obrigatórias: o GCC injeta chamadas a estas funções por otimização) */
extern size_t strlen(const char *str);
extern int strcmp(const char *s1, const char *s2);
extern void *memset(void *s, int c, size_t n);
extern void *memcpy(void *dst, const void *src, size_t n);

/* Gestão de Memória Física (PMM para Alocação de Frames) */
extern unsigned long pmm_alloc_page(void);
extern void pmm_free_page(unsigned long phys_address);
extern unsigned long pmm_alloc_pages(unsigned long count);
extern void pmm_free_pages(unsigned long phys_address, unsigned long count);


/* Gestão de Memória Virtual e Mapeamento I/O (MMIO para Drivers) */
extern void* vmm_map_device(unsigned long phys_addr, unsigned long size);
extern void* vmm_scratch_map(unsigned long phys_addr);
extern uintptr_t vmm_get_physical(uintptr_t virtual_address);

/* Alocação Síncrona na Pool DMA */
extern void* pool_alloc(size_t size);
extern void pool_free(void* virt_addr, size_t size);

/* Registo de IRQ com suporte a MSI (Kernel API) */
extern void kapi_register_irq_handler(void *dev, void (*fuc)(void));

/* Subsistema de Ficheiros Virtual (VFS Filesystem Drivers) */
extern int vfs_register_filesystem(void *fs);

/* Subsistema de Rede do Kernel (Network Stack / Driver API) */
extern int net_driver_register(const uint8_t *mac_addr, void *ops_table);
extern int net_driver_receive(const void *buffer, uint32_t packet_size);

/* Barramento PCI (Configuração e Ativação de Hardware) */
/* (Certifica-te de que pci_device_t está acessível via kmod.h ou inclui o teu pci.h) */
extern uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
extern void pci_config_write_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t data);
extern void pci_enable_mmio_busmastering(void *dev); /* Mapeado como void* ou pci_device_t* se declarado */
extern int pci_load_devices_by_class(uint8_t class_code, uint8_t subclass_code, int (*init_cb)(void *dev));
/*
 * Tabela estática contendo os símbolos públicos do Kernel.
 * Cada entrada associa uma cadeia de caracteres ao endereço real da função.
 */
static kernel_symbol_t kernel_symtab[] = {
    {"kprintf",                         (uintptr_t)kprintf2},
    {"kmalloc",                         (uintptr_t)kmalloc},
    {"kfree",                           (uintptr_t)kfree},

    /* --- Funções Críticas de String/Memória --- */
    {"strlen",                          (uintptr_t)strlen},
    {"strcmp",                          (uintptr_t)strcmp},
    {"memset",                          (uintptr_t)memset},
    {"memcpy",                          (uintptr_t)memcpy},

    /* --- Gestor de Memória Física (PMM) --- */
    {"pmm_alloc_page",                  (uintptr_t)pmm_alloc_page},
    {"pmm_free_page",                   (uintptr_t)pmm_free_page},
    {"pmm_alloc_pages",                 (uintptr_t)pmm_alloc_pages},
    {"pmm_free_pages",                  (uintptr_t)pmm_free_pages},

    /* --- Mapeamento Virtual Baseado em Hardware (MMIO) --- */
    {"vmm_map_device",                  (uintptr_t)vmm_map_device},
    {"vmm_scratch_map",                 (uintptr_t)vmm_scratch_map},
    {"vmm_get_physical",                (uintptr_t)vmm_get_physical},
    
    /* --- Gestão de Alocação de Páginas na Pool --- */
    {"pool_alloc",                      (uintptr_t)pool_alloc},
    {"pool_free",                       (uintptr_t)pool_free},

    /* --- API do Kernel para Interrupções Avançadas (MSI) --- */
    {"kapi_register_irq_handler",       (uintptr_t)kapi_register_irq_handler},

    /* --- Gestão e Registo de Novos Sistemas de Ficheiros --- */
    {"vfs_register_filesystem",         (uintptr_t)vfs_register_filesystem},

    /* --- Abstração da Pilha de Rede (TCP/IP / Ethernet Layer) --- */
    {"net_driver_register",             (uintptr_t)net_driver_register},
    {"net_driver_receive",              (uintptr_t)net_driver_receive},
    
    /* --- Subsistema de Controlo de Dispositivos PCI --- */
    {"pci_config_read_dword",           (uintptr_t)pci_config_read_dword},
    {"pci_config_write_dword",          (uintptr_t)pci_config_write_dword},
    {"pci_enable_mmio_busmastering",    (uintptr_t)pci_enable_mmio_busmastering},
    {"pci_load_devices_by_class",       (uintptr_t)pci_load_devices_by_class},
    
    /* Sentinela: fim da tabela de símbolos */
    {NULL, 0}
};

/*
 * Procura um símbolo pelo nome na tabela global do Kernel.
 * Retorna o endereço virtual do símbolo ou 0 se não for encontrado.
 */
uintptr_t kmod_find_symbol(const char *name) 
{
    if (!name) return 0;

    for (size_t i = 0; kernel_symtab[i].name != NULL; i++) 
    {
        if (strcmp(kernel_symtab[i].name, name) == 0) 
        {
            return kernel_symtab[i].address;
        }
    }

    return 0; 
}