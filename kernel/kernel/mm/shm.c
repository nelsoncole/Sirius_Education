/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: shm.c
 *    Description: Implementação do Alocador Pool para memória compartilhada.
 *                 Totalmente corrigido contra estouros de buffers e leaks de PMM.
  * 
 *         Author: Nelson Cole
 *   Created Date: 18/10/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 19/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/mm/pmm.h>
#include <kernel/kpaging.h>
#include <kernel/kvmm.h>
#include <kernel/klib.h>
#include <kernel/kernel/mm/shm.h>
#include <kernel/kernel/core/panic.h>

uint32_t *shm_bitmap;
volatile int shm_lock = 0; 

static void shm_lock_acquire(void) {
    while (__sync_lock_test_and_set(&shm_lock, 1));
}

static void shm_lock_release(void) {
    __sync_lock_release(&shm_lock);
}

void shm_init(void) {
    size_t num_dwords = (USER_SHM_MAX_PAGES + 31) / 32;
    size_t alloc_size = num_dwords * sizeof(uint32_t);
    
    shm_bitmap = (uint32_t *)pool_alloc(alloc_size);
    if (!shm_bitmap) {
        kernel_panic("[VMM SHM] Erro: Falha ao alocar o bitmap do subsistema.\n");
    }

    memset(shm_bitmap, 0, alloc_size);

    PML4_TABLE* pml4 = (PML4_TABLE*)PML4_ADDRESS;

    unsigned long new_pdpt_phys = pmm_alloc_page();
    if (!new_pdpt_phys)
    {
        kernel_panic("[VMM SHM] error: falta memoria fisica\n");
    }

    PAGE_DIRECTORY_POINTER_TABLE *pdpt = vmm_scratch_map(new_pdpt_phys);
    memset(pdpt, 0, PAGE_SIZE);
    
    pml4[224].p     = 1;
    pml4[224].rw    = 1;
    pml4[224].us    = 1;
    pml4[224].pwt   = 0;
    pml4[224].pcd   = 0;
    pml4[224].rs1   = 0;
    pml4[224].ign   = 0;
    pml4[224].phy_addr_pdpt = (new_pdpt_phys >> 12);
    pml4[224].rs2   = 0;

    kprintf("[VMM SHM] Sub-sistema de alocação Pool shm inicializado com sucesso (%d MB).\n", 
            (USER_SHM_MAX_PAGES * PAGE_SIZE) / (1024 * 1024));
}

static long shm_find_free_region(size_t num_pages) {
    size_t continuous_found = 0;
    size_t start_bit = 0;

    for (size_t i = 0; i < USER_SHM_MAX_PAGES; i++) {
        int bit_is_set = shm_bitmap[i / 32] & (1U << (i % 32));

        if (!bit_is_set) {
            if (continuous_found == 0) {
                start_bit = i;
            }
            continuous_found++;
            
            if (continuous_found == num_pages) {
                return (long)start_bit;
            }
        } else {
            continuous_found = 0;
        }
    }

    return -1;
}

static void shm_set_bits(size_t start_bit, size_t num_pages) {
    for (size_t i = start_bit; i < start_bit + num_pages; i++) {
        shm_bitmap[i / 32] |= (1U << (i % 32));
    }
}

static void shm_clear_bits(size_t start_bit, size_t num_pages) {
    for (size_t i = start_bit; i < start_bit + num_pages; i++) {
        shm_bitmap[i / 32] &= ~(1U << (i % 32));
    }
}

void* shm_alloc(size_t size, int flag) {
    if (size == 0) return NULL;

    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    PML4_TABLE* pml4 = (PML4_TABLE*)PML4_ADDRESS;

    if (num_pages == 0 || num_pages > USER_SHM_MAX_PAGES) {
        return NULL;
    }
  
    shm_lock_acquire();

    long start_bit = shm_find_free_region(num_pages);
    if (start_bit == -1) {
        kprintf("[VMM SHM] Erro: Falha ao encontrar %d páginas virtuais contíguas.\n", num_pages);
        shm_lock_release();
        return NULL;
    }
    uintptr_t base_va = USER_SHM_VIRTUAL_BASE + (start_bit * PAGE_SIZE);

    uintptr_t phys = pmm_alloc_pages(num_pages);
    if (!phys) {
        kprintf("[VMM SHM] Erro crítico: Falha ao alocar %d frames físicos.\n", num_pages);
        shm_lock_release();
        return NULL;
    }

    for (size_t i = 0; i < num_pages; i++) {
        uintptr_t va = base_va + (i * PAGE_SIZE);
        uintptr_t pa = phys + (i * PAGE_SIZE);
        #define PAGE_USER_FLAGS         (0x1 | 0x2 | 0x4)
        vmm_map_page(pml4, va, pa, flag);
    }
    
    shm_set_bits(start_bit, num_pages);
    
    shm_lock_release();
    return (void*)base_va;
}

void shm_free(void* ptr, size_t size) {
    if (!ptr || size == 0) return;

    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t base_va = (uintptr_t)ptr;
    PML4_TABLE* pml4 = (PML4_TABLE*)PML4_ADDRESS;

    if (base_va < USER_SHM_VIRTUAL_BASE || num_pages == 0) return;

    size_t start_bit = (base_va - USER_SHM_VIRTUAL_BASE) / PAGE_SIZE;
    if ((start_bit + num_pages) > USER_SHM_MAX_PAGES) return;

    shm_lock_acquire(); 

    for (size_t i = 0; i < num_pages; i++) {
        uintptr_t va = base_va + (i * PAGE_SIZE);
        uintptr_t pa = vmm_get_physical_address(pml4, va);
        
        vmm_unmap_page(pml4, va);
        
        if (pa) {
            pmm_free_pages(pa, 1); 
        }
    }

    shm_clear_bits(start_bit, num_pages);
    
    shm_lock_release();
}

unsigned long shm_virtual_to_physical(void* virt_addr) {
    uintptr_t va = (uintptr_t)virt_addr;
    PML4_TABLE* pml4 = (PML4_TABLE*)PML4_ADDRESS;

    if (va < USER_SHM_VIRTUAL_BASE || va >= (USER_SHM_VIRTUAL_BASE + (USER_SHM_MAX_PAGES * PAGE_SIZE))) {
        return 0;
    }

    uintptr_t page_va = va & ~(PAGE_SIZE - 1);
    
    shm_lock_acquire();
    uintptr_t pa = vmm_get_physical_address(pml4, page_va);
    shm_lock_release();
    
    if (!pa) return 0;

    uintptr_t offset = va & (PAGE_SIZE - 1);
    return (unsigned long)(pa + offset);
}