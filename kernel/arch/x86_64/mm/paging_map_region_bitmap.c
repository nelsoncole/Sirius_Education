/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: paging_map_region_bitmap.c
 *    Description: Inicialização e mapeamento sequencial e explícito do bitmap 
 *                 de memória física (PMM) usando o bloco de Page Tables.
 *                 Garante escrita isolada iniciando a partir do índice zero
 *                 da tabela PD dedicada (PD_BITMAP_ADDRESS).
 * 
 *         Author: Nelson Cole
 *   Created Date: 29/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 11/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/mm/paging.h>
#include <kernel/kernel.h>
#include <kernel/lib/stdint.h>

unsigned long paging_map_region_bitmap(BOOT_INFO *boot_info, 
    unsigned long bitmap_phys_addr, 
    unsigned long ram_size_bytes)
{
    unsigned long bitmap_size;
    unsigned long bitmap_pages;
    
    PML4_TABLE *pml4;
    PAGE_DIRECTORY_POINTER_TABLE *pdpt;
    PAGE_DIRECTORY *pd_bitmap;

    pml4      = (PML4_TABLE *)PML4_ADDRESS;
    pdpt      = (PAGE_DIRECTORY_POINTER_TABLE *)PDPT_256_ADDRESS;
    pd_bitmap = (PAGE_DIRECTORY *)PD_BITMAP_ADDRESS; 

    unsigned long PDPT_PHYSICAL = boot_info->KernelAddress + PDPT_256_PHYSICAL_OFFSET;
    unsigned long PD_BITMAP_PHYSICAL = boot_info->KernelAddress + PD_BITMAP_PHYSICAL_OFFSET;
    unsigned long PT_PHYSICAL   = boot_info->KernelAddress + PT_PHYSICAL_OFFSET;

    bitmap_size = ram_size_bytes;
    bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /*
     * ========================================================
     * CONEXÃO DOS RAMOS SUPERIORES (SEM SHIFTS PARA BITFIELDS)
     * ========================================================
     */
    pml4[256].p  = 1;
    pml4[256].rw = 1;
    pml4[256].us = 0;
    pml4[256].phy_addr_pdpt = PDPT_PHYSICAL >> 12;

    unsigned long bitmap_virtual = KERNEL_BITMAP_VIRTUAL_BASE; 
    unsigned long bitmap_pdpt_index = (bitmap_virtual >> 30) & 0x1FF; 

    pdpt[bitmap_pdpt_index].p  = 1;
    pdpt[bitmap_pdpt_index].rw = 1;
    pdpt[bitmap_pdpt_index].us = 0;
    pdpt[bitmap_pdpt_index].phy_addr_pd = PD_BITMAP_PHYSICAL >> 12;

    unsigned long bitmap_pt_start = g_next_pt_number;

    /*
     * ========================================================
     * MAPEAR BITMAP
     * ========================================================
     */
    for (unsigned long page = 0; page < bitmap_pages; page++)
    {
        unsigned long pt_number = bitmap_pt_start + (page / 512);

        if (pt_number >= NUM_PT_TABLES)
        {
            break;
        }

        unsigned long physical = bitmap_phys_addr + page * PAGE_SIZE;
        unsigned long virtual_addr = KERNEL_BITMAP_VIRTUAL_BASE + page * PAGE_SIZE;

        unsigned long pd_index = (page / 512) & 0x1FF;
        unsigned long pt_index = page & 0x1FF;

        unsigned long current_pt_physical = PT_PHYSICAL + pt_number * PAGE_SIZE;

        /*
         * ====================================================
         * PD_BITMAP -> PT 
         * ====================================================
         */
        pd_bitmap[pd_index].p  = 1;
        pd_bitmap[pd_index].rw = 1;
        pd_bitmap[pd_index].us = 0;
        pd_bitmap[pd_index].ps = 0;
        pd_bitmap[pd_index].phy_addr_pt = current_pt_physical >> 12;

        /*
         * ====================================================
         * PT ENTRY -> CORREÇÃO DA ARITMÉTICA DE PONTEIROS
         * ====================================================
         * O cast para (uintptr_t) força a soma a ser feita em bytes puros,
         * evitando que o compilador multiplique o deslocamento por sizeof(PAGE_TABLE).
         */
        PAGE_TABLE *local_pt = (PAGE_TABLE *)((uintptr_t)PT_ADDRESS + (pt_number * PAGE_SIZE));

        local_pt[pt_index].p  = 1;
        local_pt[pt_index].rw = 1;
        local_pt[pt_index].us = 0;
        local_pt[pt_index].frames = physical >> 12;
        local_pt[pt_index].nx = 1; 
        
        __asm__ volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
    }

    // Atualiza o contador de tabelas cheias
    g_next_pt_number += (bitmap_pages + 511UL) / 512UL;

    return KERNEL_BITMAP_VIRTUAL_BASE;
}
