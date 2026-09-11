/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vmm_scratch_window.c
 *    Description: Implementação das janelas de mapping temporário (Scratch 
 *                 Windows) lendo diretamente o endereço virtual da PT.
 * 
 *         Author: Nelson Cole
 *   Created Date: 30/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 10/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/mm/paging.h>
#include <kernel/arch/x86_64/mm/vmm.h>
#include <kernel/boot_info.h>
#include <kernel/kernel/core/panic.h>
#include <kernel/klib.h>

/*
 * ============================================================================
 * CONFIGURAÇÃO INICIAL DA JANELA TEMPORÁRIA (VMM SCRATCH WINDOW SETUP)
 * ============================================================================
 */
// VARIÁVEL GLOBAL: Mantém o endereço virtual da PT dedicada ao Scratch no array linear do Kernel
PAGE_TABLE *g_window_pt = 0;

/*
 * INICIALIZAÇÃO FIXA DO SUBSISTEMA DE SCRATCH ISOLADO
 * ------------------------------------------------------------------------
 * Baseado na lógica de paginação estática do Kernel.
 * Configura as bases virtuais estáveis para os slots 496, 497 e 498.
 */
void vmm_scratch_setup(void) {
    // 1. Descobre a distância em bytes a partir da base VMM_SCRATCH_ISOLATED_BASE
    unsigned long distance_bytes = VMM_SCRATCH_ISOLATED_BASE - KERNEL_VIRTUAL_BASE;

    // 2. Transforma a distância em número total de páginas de 4 KB
    unsigned long total_pages = distance_bytes / PAGE_SIZE;

    // 3. Cada PT gerencia exatamente 512 páginas (2 MB).
    unsigned long pt_number = total_pages / 512;

    // CORREÇÃO: Atribuição direta à variável global (Removido o tipo local para evitar Shadowing)
    // O cast para (uintptr_t) garante a aritmética correta por bytes antes de converter para ponteiro
    g_window_pt = (PAGE_TABLE *)((uintptr_t)PT_KERNEL_ADDRESS + (pt_number * PAGE_SIZE));

    // 4. Limpeza cirúrgica de segurança dos nossos 3 slots específicos para evitar lixo
    unsigned long base_pt_idx = GET_PT_INDEX(VMM_SCRATCH_WINDOW); // Índice 496
    unsigned long long *raw_pt = (unsigned long long *)g_window_pt;
    
    raw_pt[base_pt_idx]     = 0; // Slot 496
    raw_pt[base_pt_idx + 1] = 0; // Slot 497
    raw_pt[base_pt_idx + 2] = 0; // Slot 498
}

/*
 * OPERAÇÃO VOLÁTIL DE TROCA DE FRAME (SCRATCH MAP PADRÃO)
 * ------------------------------------------------------------------------
 */
void* vmm_scratch_map(unsigned long phys_addr) {
    PAGE_TABLE* local_pt = g_window_pt;
    unsigned long pt_idx = GET_PT_INDEX(VMM_SCRATCH_WINDOW); // Resulta em Índice 496

    // Configuração local direta sem necessidade de pt_entry linear global
    local_pt[pt_idx].p      = 1;
    local_pt[pt_idx].rw     = 1;
    local_pt[pt_idx].us     = 0;
    local_pt[pt_idx].frames = phys_addr >> 12;

    __asm__ volatile("invlpg (%0)" :: "r"(VMM_SCRATCH_WINDOW) : "memory");

    return (void*)VMM_SCRATCH_WINDOW;
}

/*
 * OPERAÇÃO VOLÁTIL EXCLUSIVA INTERNA (SCRATCH MAP INTERNAL)
 * ------------------------------------------------------------------------
 */
void* vmm_scratch_map_internal(unsigned long phys_addr, int window) {
    PAGE_TABLE* local_pt = g_window_pt;
    
    unsigned long base_pt_idx = GET_PT_INDEX(VMM_SCRATCH_WINDOW_0); // Resulta em Índice 497
    unsigned long pt_idx      = base_pt_idx + (window ? 1 : 0);     // 497 (window 0) ou 498 (window 1)
    unsigned long scratch_va  = window ? VMM_SCRATCH_WINDOW_1 : VMM_SCRATCH_WINDOW_0;

    // Configuração local direta imune a overflows de tabelas
    local_pt[pt_idx].p      = 1;
    local_pt[pt_idx].rw     = 1;
    local_pt[pt_idx].us     = 0;
    local_pt[pt_idx].frames = phys_addr >> 12;

    __asm__ volatile("invlpg (%0)" :: "r"(scratch_va) : "memory");

    return (void*)scratch_va;
}
