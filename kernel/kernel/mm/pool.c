/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pool.c
 *    Description: Implementação do Alocador Pool de Alto Nível do Kernel.
 *                 Permite a alocação dinâmica e reutilizável de memória
 *                 virtual contígua mapeada a frames físicos para E/S e DMA.
 * 
 *         Author: Nelson Cole
 *   Created Date: 09/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 10/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/mm/vmm.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/kpaging.h>
#include <kernel/kvmm.h>
#include <kernel/kernel/mm/pool.h>
#include <kernel/klib.h>

// Definição dos bits de paginação padrão para x86_64 caso não estejam no header
#define PAGE_PRESENT  (1ULL << 0) // Bit 0: Página presente na RAM
#define PAGE_WRITABLE (1ULL << 1) // Bit 1: Permissão de Escrita (Read/Write)

// Um bitmap simples para gerir os endereços virtuais desta janela
uint32_t pool_bitmap[KERNEL_POOL_MAX_PAGES / 32] = {0};

/*
 * INICIALIZAÇÃO DO POOL DE MEMÓRIA VIRTUAL DO KERNEL
 */
void pool_init(void) {
    // Garante que cada slot de 32-bits do bitmap é zerado explicitamente
    for (size_t i = 0; i < (KERNEL_POOL_MAX_PAGES / 32); i++) {
        pool_bitmap[i] = 0;
    }
    
    kprintf("[VMM Pool] Sub-sistema de alocação Pool inicializado com sucesso (%d MB).\n", 
            (KERNEL_POOL_MAX_PAGES * PAGE_SIZE) / (1024 * 1024));
}

/*
 * FUNÇÕES DE SUPORTE INTERNAS PARA GESTÃO DO ENDEREÇO VIRTUAL DO POOL
 * ------------------------------------------------------------------------
 */

// Procura uma sequência de bits livres (0) contíguos no bitmap
static long pool_find_free_region(size_t num_pages) {
    size_t continuous_found = 0;
    size_t start_bit = 0;

    for (size_t i = 0; i < KERNEL_POOL_MAX_PAGES; i++) {
        int bit_is_set = pool_bitmap[i / 32] & (1U << (i % 32));

        if (!bit_is_set) {
            if (continuous_found == 0) {
                start_bit = i; // Define o início da nova sequência candidata
            }
            continuous_found++;
            
            if (continuous_found == num_pages) {
                return (long)start_bit; // Encontrou a região contígua com sucesso
            }
        } else {
            // Sequência quebrada: reinicia a contagem para o próximo bit livre
            continuous_found = 0;
        }
    }

    return -1; // Sem espaço contíguo suficiente disponível
}

static void pool_set_bits(size_t start_bit, size_t num_pages) {
    for (size_t i = start_bit; i < start_bit + num_pages; i++) {
        pool_bitmap[i / 32] |= (1U << (i % 32));
    }
}

static void pool_clear_bits(size_t start_bit, size_t num_pages) {
    for (size_t i = start_bit; i < start_bit + num_pages; i++) {
        pool_bitmap[i / 32] &= ~(1U << (i % 32));
    }
}

/*
 * APIS PRINCIPAIS DO ALOCADOR POOL
 * ------------------------------------------------------------------------
 */

void* pool_alloc(size_t size) {
    if (size == 0)
    {
        return NULL;
    }

    // 1. CORREÇÃO: Cálculo exato e seguro do número de páginas (arredondado para cima)
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    PML4_TABLE* pml4 = (PML4_TABLE*)PML4_ADDRESS;
    if (num_pages == 0 || num_pages > KERNEL_POOL_MAX_PAGES) {
        return NULL;
    }
    // 1. Procura espaço virtual contíguo livre no Pool
    long start_bit = pool_find_free_region(num_pages);
    if (start_bit == -1) {
        kprintf("[VMM Pool] Erro: Falha ao encontrar %d páginas virtuais contíguas.\n", num_pages);
        return NULL;
    }
    uintptr_t base_va = KERNEL_POOL_VIRTUAL_BASE + (start_bit * PAGE_SIZE);

    // 2. Aloca os frames físicos e mapeia-os na janela virtual encontrada
    uintptr_t phys = pmm_alloc_pages(num_pages);
    if (!phys)
    {
        kprintf("[VMM Pool] Erro crítico: PMM sem memória física durante a alocação.\n");
        return NULL;
    }

    for (size_t i = 0; i < num_pages; i++) {
        uintptr_t va = base_va + (i * PAGE_SIZE);
        uintptr_t pa = phys + (i * PAGE_SIZE);
        // vmm_map_page já trata do mapeamento e da invalidação da TLB internamente
        vmm_map_page(pml4, va, pa, PAGE_PRESENT | PAGE_WRITABLE);
    }
    // 3. Sinaliza a ocupação no bitmap
    pool_set_bits(start_bit, num_pages);


    return (void*)base_va;
}

void pool_free(void* ptr, size_t size) {

    if (size == 0) {
        return;
    }

    // 1. CORREÇÃO: Cálculo exato e seguro do número de páginas (arredondado para cima)
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    uintptr_t base_va = (uintptr_t)ptr;

    PML4_TABLE* pml4 = (PML4_TABLE*)PML4_ADDRESS;

    // Validações básicas de segurança
    if (base_va < KERNEL_POOL_VIRTUAL_BASE || num_pages == 0) {
        return;
    }

    size_t start_bit = (base_va - KERNEL_POOL_VIRTUAL_BASE) / PAGE_SIZE;
    if ((start_bit + num_pages) > KERNEL_POOL_MAX_PAGES) {
        return;
    }

    // Desmapeia as páginas e liberta os frames físicos associados
    for (size_t i = 0; i < num_pages; i++) {
        uintptr_t va = base_va + (i * PAGE_SIZE);

        vmm_unmap_page(pml4, va);
    }

    // Liberta o espaço no bitmap
    pool_clear_bits(start_bit, num_pages);
}


/*
 * CONVERSÃO DE ENDEREÇO VIRTUAL PARA FÍSICO (Para operações de E/S e DMA)
 * ------------------------------------------------------------------------
 * Traduz um endereço virtual pertencente à janela do Pool para o seu
 * respetivo endereço físico na RAM.
 */
unsigned long pool_virtual_to_physical(void* virt_addr) {
    uintptr_t va = (uintptr_t)virt_addr;

    PML4_TABLE* pml4 = (PML4_TABLE*)PML4_ADDRESS;

    // Garante que o endereço está estritamente dentro da janela do Pool do Kernel
    if (va < KERNEL_POOL_VIRTUAL_BASE || va >= (KERNEL_POOL_VIRTUAL_BASE + (KERNEL_POOL_MAX_PAGES * PAGE_SIZE))) {
        return 0; // Retorna 0 como endereço físico inválido
    }

    // Isola o endereço base da página (alinhado a 4KB)
    uintptr_t page_va = va & ~(PAGE_SIZE - 1);
    
    // Obtém o frame físico base através do VMM do sistema
    uintptr_t pa = vmm_get_physical_address(pml4, page_va);
    if (!pa) {
        return 0; // Mapeamento não encontrado ou página inválida
    }

    // Preserva o deslocamento original (offset) dentro da página e adiciona-o ao frame físico
    uintptr_t offset = va & (PAGE_SIZE - 1);
    
    return (unsigned long)(pa + offset);
}