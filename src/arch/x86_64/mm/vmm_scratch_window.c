/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vmm_scratch_window.c
 *    Description: Implementação da janela de mapeamento temporário (Scratch 
 *                 Window) de 4 KB para manipulação de páginas físicas no VMM.
 * 
 *         Author: Nelson Cole
 *   Created Date: 30/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 30/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/mm/paging.h>
#include <kernel/arch/mm/vmm.h>
#include <kernel/boot_info.h>

/*
 * ============================================================================
 * CONFIGURAÇÃO INICIAL DA JANELA TEMPORÁRIA (VMM SCRATCH WINDOW)
 * ============================================================================
 * 
 * Prepara a infraestrutura de tabelas necessária para suportar a janela.
 * Garante que a PT que gerencia o endereço 0xFFFFFFFF803FF000UL esteja alocada
 * e vinculada estritamente ao Índice 511 da PML4, Índice 510 da PDPT e Índice 1 da PD.
 */
void vmm_scratch_setup(void) {
    PML4_TABLE* pml4                   = (PML4_TABLE*)PML4_ADDRESS;
    PAGE_DIRECTORY_POINTER_TABLE* pdpt = (PAGE_DIRECTORY_POINTER_TABLE*)PDPT_ADDRESS;
    PAGE_DIRECTORY* pd_kernel          = (PAGE_DIRECTORY*)PD_KERNEL_ADDRESS;
    PAGE_TABLE* pt                     = (PAGE_TABLE*)PT_ADDRESS;

    // Índices exatos extraídos de 0xFFFFFFFF803FF000UL
    unsigned long pml4_idx = GET_PML4_INDEX(VMM_SCRATCH_WINDOW); // Índice 511
    unsigned long pdpt_idx = GET_PDPT_INDEX(VMM_SCRATCH_WINDOW); // Índice 510
    unsigned long pd_idx   = GET_PD_INDEX(VMM_SCRATCH_WINDOW);   // Índice 1

    unsigned long PT_PHYSICAL = g_boot_info->KernelAddress + PT_PHYSICAL_OFFSET;

    /*
     * 1. Garante que a PML4 aponta para a PDPT estável do Kernel
     */
    if (!pml4[pml4_idx].p) {
        unsigned long PDPT_PHYSICAL = g_boot_info->KernelAddress + PDPT_PHYSICAL_OFFSET;
        pml4[pml4_idx].p = 1;
        pml4[pml4_idx].rw = 1;
        pml4[pml4_idx].us = 0;
        pml4[pml4_idx].phy_addr_pdpt = PDPT_PHYSICAL >> 12;
    }

    /*
     * 2. Garante que a PDPT aponta para o Diretório do Kernel (PD_KERNEL)
     */
    if (!pdpt[pdpt_idx].p) {
        unsigned long PD_KERNEL_PHYSICAL = g_boot_info->KernelAddress + PD_KERNEL_PHYSICAL_OFFSET;
        pdpt[pdpt_idx].p = 1;
        pdpt[pdpt_idx].rw = 1;
        pdpt[pdpt_idx].us = 0;
        pdpt[pdpt_idx].phy_addr_pd = PD_KERNEL_PHYSICAL >> 12;
    }

    /*
     * 3. Vincula o Índice 1 da PD a uma nova Tabela de Páginas (PT) estática
     */
    if (!pd_kernel[pd_idx].p) {
        unsigned long pt_number = g_next_pt_number;
        if (pt_number >= NUM_PT_TABLES) return; // Limite de segurança

        unsigned long current_pt_physical = PT_PHYSICAL + pt_number * PAGE_SIZE;

        pd_kernel[pd_idx].p = 1;
        pd_kernel[pd_idx].rw = 1;
        pd_kernel[pd_idx].us = 0;
        pd_kernel[pd_idx].ps = 0;
        pd_kernel[pd_idx].phy_addr_pt = current_pt_physical >> 12;

        // Limpa a nova PT estática criada para evitar lixo de memória
        unsigned long pt_entry_start = pt_number * 512;
        unsigned long long* raw_pt = (unsigned long long*)pt;
        for (int i = 0; i < 512; i++) {
            raw_pt[pt_entry_start + i] = 0;
        }

        g_next_pt_number++;
    }
}

/*
 * OPERAÇÃO VOLÁTIL DE TROCA DE FRAME (SCRATCH MAP)
 * ------------------------------------------------------------------------
 * Substitui instantaneamente o frame físico mapeado na janela de 4 KB.
 * Invalida a cache TLB para atualizar o acesso virtual imediatamente.
 */
void* vmm_scratch_map(unsigned long phys_addr) {
    PAGE_DIRECTORY* pd_kernel = (PAGE_DIRECTORY*)PD_KERNEL_ADDRESS;
    PAGE_TABLE* pt            = (PAGE_TABLE*)PT_ADDRESS;

    unsigned long pd_idx = GET_PD_INDEX(VMM_SCRATCH_WINDOW); // Índice 1
    unsigned long pt_idx = GET_PT_INDEX(VMM_SCRATCH_WINDOW); // Índice 511

    unsigned long PT_PHYSICAL = g_boot_info->KernelAddress + PT_PHYSICAL_OFFSET;

    // 1. Recupera o número absoluto da PT vinculada ao índice 1 do Diretório do Kernel
    unsigned long current_pt_physical = (unsigned long)pd_kernel[pd_idx].phy_addr_pt << 12;
    unsigned long pt_number = (current_pt_physical - PT_PHYSICAL) / PAGE_SIZE;

    // 2. Calcula a entrada exata da PTE (PT Número * 512 + Entrada 511)
    unsigned long pt_entry = (pt_number * 512) + pt_idx;

    // 3. Injeta cirurgicamente o novo endereço físico da RAM solicitado
    pt[pt_entry].p = 1;
    pt[pt_entry].rw = 1;
    pt[pt_entry].us = 0;
    pt[pt_entry].frames = phys_addr >> 12;

    // 4. Força o processador a limpar a cache para o endereço virtual específico
    __asm__ volatile("invlpg (%0)" :: "r"(VMM_SCRATCH_WINDOW) : "memory");

    // Retorna o ponteiro virtual fixo pronto para escrita/leitura
    return (void*)VMM_SCRATCH_WINDOW;
}
