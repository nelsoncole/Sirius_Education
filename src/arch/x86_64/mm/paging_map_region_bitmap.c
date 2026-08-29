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
 *  Modified Date: 29/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include "paging.h"
#include <kernel/kernel.h>

unsigned long paging_map_region_bitmap(BOOT_INFO *boot_info, 
    unsigned long bitmap_phys_addr, 
    unsigned long ram_size_bytes)
{
    unsigned long bitmap_size;
    unsigned long bitmap_pages;
    
    PML4_TABLE *pml4;
    PAGE_DIRECTORY_POINTER_TABLE *pdpt;
    PAGE_DIRECTORY *pd_bitmap;
    PAGE_TABLE *pt;

    pml4      = (PML4_TABLE *)PML4_ADDRESS;
    pdpt      = (PAGE_DIRECTORY_POINTER_TABLE *)PDPT_ADDRESS;
    pd_bitmap = (PAGE_DIRECTORY *)PD_BITMAP_ADDRESS; // Uso estrito do diretório exclusivo do Bitmap
    pt        = (PAGE_TABLE *)PT_ADDRESS;

    unsigned long PDPT_PHYSICAL = boot_info->KernelAddress + PDPT_PHYSICAL_OFFSET;
    unsigned long PD_BITMAP_PHYSICAL = boot_info->KernelAddress + PD_BITMAP_PHYSICAL_OFFSET;
    unsigned long PT_PHYSICAL   = boot_info->KernelAddress + PT_PHYSICAL_OFFSET;

   
    // Tamanho em bytes
    bitmap_size = ram_size_bytes;

    /*
     * Quantidade de páginas de 4 KB necessárias para cobrir o Bitmap.
     */
    bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /*
     * ========================================================
     * PML4 -> PARTILHADO COM O KERNEL E VÍDEO (Índice Higher Half)
     * ========================================================
     */
    pml4[256].p  = 1;
    pml4[256].rw = 1;
    pml4[256].us = 0;

    pml4[256].phy_addr_pdpt = PDPT_PHYSICAL >> 12;

    /*
     * ========================================================
     * CALCULAR ÍNDICES DO BASE VIRTUAL DO BITMAP
     * ========================================================
     */
    unsigned long bitmap_virtual = KERNEL_BITMAP_VIRTUAL_BASE; // 0xFFFF800000000000

    unsigned long bitmap_pdpt_index = (bitmap_virtual >> 30) & 0x1FF; // Será índice 0

    /*
     * ========================================================
     * PDPT -> DIRECIONA O ÍNDICE 0 PARA A PD EXCLUSIVA DO BITMAP
     * ========================================================
     */
    pdpt[bitmap_pdpt_index].p  = 1;
    pdpt[bitmap_pdpt_index].rw = 1;
    pdpt[bitmap_pdpt_index].us = 0;

    // Vincula a entrada 0 da PDPT à tabela PD_BITMAP física dedicada
    pdpt[bitmap_pdpt_index].phy_addr_pd = PD_BITMAP_PHYSICAL >> 12;

    /*
     * ========================================================
     * PRIMEIRA PT DISPONÍVEL NO MOMENTO
     * ========================================================
     */
    unsigned long bitmap_pt_start = g_next_pt_number;

    /*
     * ========================================================
     * MAPEAR BITMAP (Escrita inicia do índice 0 da PD)
     * ========================================================
     */
    for (unsigned long page = 0; page < bitmap_pages; page++)
    {
        unsigned long pt_number = bitmap_pt_start + (page / 512);

        /*
         * Verificar limite das PTs (252 disponíveis).
         */
        if (pt_number >= NUM_PT_TABLES)
        {
            break;
        }

        /* Endereço físico do bloco do bitmap na RAM */
        unsigned long physical = bitmap_phys_addr + page * PAGE_SIZE;

        /* Endereço virtual contínuo */
        unsigned long virtual_addr = KERNEL_BITMAP_VIRTUAL_BASE + page * PAGE_SIZE;

        /*
         * COMO A PD É EXCLUSIVA:
         * Calculamos os índices de forma sequencial com base no número da página atual.
         * Garante que a primeira iteração (page = 0) use obrigatoriamente pd_index = 0 e pt_index = 0.
         */
        unsigned long pd_index = (page / 512) & 0x1FF;
        unsigned long pt_index = page & 0x1FF;

        /* Endereço físico da PT atual dentro do array g_pt */
        unsigned long current_pt_physical = PT_PHYSICAL + pt_number * PAGE_SIZE;

        /*
         * ====================================================
         * PD_BITMAP -> PT (Preenche a partir do índice 0)
         * ====================================================
         */
        pd_bitmap[pd_index].p  = 1;
        pd_bitmap[pd_index].rw = 1;
        pd_bitmap[pd_index].us = 0;
        pd_bitmap[pd_index].ps = 0;

        pd_bitmap[pd_index].phy_addr_pt = current_pt_physical >> 12;

        /*
         * ====================================================
         * PT ENTRY -> Injeta o frame físico da RAM
         * ====================================================
         */
        unsigned long pt_entry = (pt_number * 512) + pt_index;

        pt[pt_entry].p  = 1;
        pt[pt_entry].rw = 1;
        pt[pt_entry].us = 0;
        pt[pt_entry].frames = physical >> 12;

        /*
         * Bitmap armazena metadados de controlo (NX = 1)
         */
        pt[pt_entry].nx = 1;
        
        // Invalida a cache TLB para este endereço virtual
        __asm__ volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
    }

        /*
     * ========================================================
     * ATUALIZAR CONTADOR GLOBAL DE TABELAS LIVRES
     * 
     * Soma 1 ao g_next_pt_number a cada bloco de 512 páginas 
     * (arredondado para cima), garantindo o alinhamento.
     * ========================================================
     */
    g_next_pt_number += (bitmap_pages + 511UL) / 512UL;

    return KERNEL_BITMAP_VIRTUAL_BASE;
}
