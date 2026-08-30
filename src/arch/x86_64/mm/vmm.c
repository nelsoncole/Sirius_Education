/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vmm.c
 *    Description: Implementação do Gerenciador de Memória Virtual (VMM).
 *                 Responsável pela manipulação das tabelas de paginação.
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

#include <kernel/arch/mm/vmm.h>
#include <kernel/kernel/mm/pmm.h>


/*
 * ============================================================================
 * CONFIGURAÇÃO E INICIALIZAÇÃO DO GERENCIADOR VIRTUAL (VMM SETUP)
 * ============================================================================
 * 
 * Prepara o ambiente de paginação definitivo para o funcionamento do Kernel.
 * Configura a Janela Temporária (Scratch) e realiza o mapeamento primitivo
 * dos 2 MB regulamentares para o Heap estável do sistema operativo.
 */
void vmm_init(void) {
    // 1. Ativa a infraestrutura da Janela Temporária de 4 KB
    vmm_scratch_setup();

    // Ponteiro para a PML4 raiz do sistema mapeada pelo bootloader
    PML4_TABLE* pml4 = (PML4_TABLE*)PML4_ADDRESS;

    /*
     * 2. MAPEAR O HEAP INICIAL DO KERNEL (2 MB)
     * ------------------------------------------------------------------------
     * Varre o intervalo correspondente a 512 páginas de 4 KB (Exatamente 2 MB),
     * alocando frames físicos reais do PMM e injetando-os no endereço virtual
     * KERNEL_HEAP_VIRTUAL_BASE com permissões de Leitura e Escrita (Flags: 0x2).
     */
    for (unsigned long offset = 0; offset < KERNEL_HEAP_INITIAL_SIZE; offset += PAGE_SIZE) {
        // Aloca uma página física do PMM para o Heap
        unsigned long phys_page = pmm_alloc_page();

        // Endereço virtual contínuo do Heap em progressão
        unsigned long virt_addr = KERNEL_HEAP_VIRTUAL_BASE + offset;

        /*
         * Injeta o mapeamento na árvore de tabelas.
         * Flags: bit 1 ativo (Leitura/Escrita), bit 2 zerado (Supervisor/Ring 0),
         * bit 63 zerado (Permite execução caso necessário, embora seja Heap).
         */
        vmm_map_page(pml4, virt_addr, phys_page, 0x2);
    }
}

/*
 * OPERAÇÃO DE ALTERAÇÃO DO DIRETÓRIO RAÍZ
 * ------------------------------------------------------------------------
 * Atualiza o registo CR3 do processador utilizando o endereço físico bruto.
 * Força a MMU a transitar imediatamente para o novo espaço virtual.
 */
void vmm_switch_pml4(unsigned long pml4_phys) {
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

/*
 * TRADUÇÃO E MAPEAMENTO DINÂMICO DE PÁGINAS (VMM MAP PAGE)
 * ------------------------------------------------------------------------
 * Mapeia um endereço virtual para um endereço físico injetando os dados
 * nas PTs e reaproveitando as estruturas hierárquicas existentes de forma
 * segura, operando modificações via Janela Temporária (Scratch Window).
 * 
 * @param pml4  Ponteiro para a tabela raíz PML4 ativa do sistema.
 * @param virt  Endereço virtual superior alvo do mapeamento.
 * @param phys  Endereço físico bruto da RAM alocado pelo PMM.
 * @param flags Atributos de proteção e privilégio da MMU x86_64:
 *              - Bit 1 (0x2) : Read/Write (1 = Escrita ativa, 0 = Apenas Leitura).
 *              - Bit 2 (0x4) : User/Supervisor (1 = Modo Usuário, 0 = Kernel/Ring 0).
 *              - Bit 63      : No-Execute (1 = Impede execução de código/NX).
 */
void vmm_map_page(PML4_TABLE* pml4, unsigned long virt, unsigned long phys, unsigned long flags) {
    unsigned long pml4_idx = GET_PML4_INDEX(virt);
    unsigned long pdpt_idx = GET_PDPT_INDEX(virt);
    unsigned long pd_idx   = GET_PD_INDEX(virt);
    unsigned long pt_idx   = GET_PT_INDEX(virt);

    unsigned char flag_rw = (flags & (1ULL << 1)) ? 1 : 0;
    unsigned char flag_us = (flags & (1ULL << 2)) ? 1 : 0;

    /*
     * 1. GARANTE A EXISTÊNCIA DA PDPT
     * ------------------------------------------------------------------------
     */
    if (!pml4[pml4_idx].p) {
        unsigned long new_table_phys = pmm_alloc_page();
        
        unsigned long long* virt_ptr = (unsigned long long*)vmm_scratch_map(new_table_phys);
        for (int i = 0; i < 512; i++) virt_ptr[i] = 0;
        
        pml4[pml4_idx].p = 1;
        pml4[pml4_idx].rw = flag_rw;
        pml4[pml4_idx].us = flag_us;
        pml4[pml4_idx].phy_addr_pdpt = (new_table_phys >> 12);
    }
    unsigned long pdpt_phys = (unsigned long)pml4[pml4_idx].phy_addr_pdpt << 12;

    /*
     * 2. GARANTE A EXISTÊNCIA DO DIRETÓRIO DE PÁGINAS (PD)
     * ------------------------------------------------------------------------
     */
    PAGE_DIRECTORY_POINTER_TABLE* pdpt = (PAGE_DIRECTORY_POINTER_TABLE*)vmm_scratch_map(pdpt_phys);
    if (!pdpt[pdpt_idx].p) {
        unsigned long new_table_phys = pmm_alloc_page();
        
        unsigned long long* virt_ptr = (unsigned long long*)vmm_scratch_map(new_table_phys);
        for (int i = 0; i < 512; i++) virt_ptr[i] = 0;
        
        // Reabre a PDPT para salvar o vínculo do novo frame físico
        pdpt = (PAGE_DIRECTORY_POINTER_TABLE*)vmm_scratch_map(pdpt_phys);
        pdpt[pdpt_idx].p = 1;
        pdpt[pdpt_idx].rw = flag_rw;
        pdpt[pdpt_idx].us = flag_us;
        pdpt[pdpt_idx].phy_addr_pd = (new_table_phys >> 12);
    }
    unsigned long pd_phys = (unsigned long)pdpt[pdpt_idx].phy_addr_pd << 12;

    /*
     * 3. GARANTE A EXISTÊNCIA DA TABELA DE PÁGINAS (PT)
     * ------------------------------------------------------------------------
     */
    PAGE_DIRECTORY* pd = (PAGE_DIRECTORY*)vmm_scratch_map(pd_phys);
    if (!pd[pd_idx].p) {
        unsigned long new_table_phys = pmm_alloc_page();
        
        unsigned long long* virt_ptr = (unsigned long long*)vmm_scratch_map(new_table_phys);
        for (int i = 0; i < 512; i++) virt_ptr[i] = 0;
        
        // Reabre a PD para salvar o vínculo do novo frame físico da PT
        pd = (PAGE_DIRECTORY*)vmm_scratch_map(pd_phys);
        pd[pd_idx].p = 1;
        pd[pd_idx].rw = flag_rw;
        pd[pd_idx].us = flag_us;
        pd[pd_idx].ps = 0; 
        pd[pd_idx].phy_addr_pt = (new_table_phys >> 12);
    }
    unsigned long pt_phys = (unsigned long)pd[pd_idx].phy_addr_pt << 12;

    /*
     * 4. CONFIGURAÇÃO FINAL DO DESCRITOR DE PÁGINA (PTE)
     * ------------------------------------------------------------------------
     */
    PAGE_TABLE* pt = (PAGE_TABLE*)vmm_scratch_map(pt_phys);
    pt[pt_idx].p = 1;
    pt[pt_idx].rw = flag_rw;
    pt[pt_idx].us = flag_us;
    pt[pt_idx].frames = (phys >> 12);
    pt[pt_idx].nx = (flags & (1ULL << 63)) ? 1 : 0;

    // Sincroniza imediatamente a cache MMU do processador para o endereço virtual alvo
    __asm__ __volatile__("invlpg (%0)" :: "r"(virt) : "memory");
}


/*
 * REMOÇÃO DE MAPEAMENTO DE PÁGINAS VIRTUAIS
 * ------------------------------------------------------------------------
 * Desativa os bits de presença, liberta o frame físico associado de volta
 * ao PMM e limpa as referências. Força o flush da TLB imediatamente.
 */
void vmm_unmap_page(PML4_TABLE* pml4, unsigned long virt) {
    unsigned long pml4_idx = GET_PML4_INDEX(virt);
    unsigned long pdpt_idx = GET_PDPT_INDEX(virt);
    unsigned long pd_idx   = GET_PD_INDEX(virt);
    unsigned long pt_idx   = GET_PT_INDEX(virt);

    // 1. Verifica se a PDPT existe
    if (!pml4[pml4_idx].p) return;
    unsigned long pdpt_phys = (unsigned long)pml4[pml4_idx].phy_addr_pdpt << 12;
    
    // 2. Verifica se o PD existe
    PAGE_DIRECTORY_POINTER_TABLE* pdpt = (PAGE_DIRECTORY_POINTER_TABLE*)vmm_scratch_map(pdpt_phys);
    if (!pdpt[pdpt_idx].p) return;
    unsigned long pd_phys = (unsigned long)pdpt[pdpt_idx].phy_addr_pd << 12;

    // 3. Verifica se a PT existe
    PAGE_DIRECTORY* pd = (PAGE_DIRECTORY*)vmm_scratch_map(pd_phys);
    if (!pd[pd_idx].p) return;
    unsigned long pt_phys = (unsigned long)pd[pd_idx].phy_addr_pt << 12;

    // 4. Mapeia a PT final para inspecionar e modificar a entrada da página
    PAGE_TABLE* pt = (PAGE_TABLE*)vmm_scratch_map(pt_phys);
    if (!pt[pt_idx].p) return;

    // Recupera o endereço físico do frame antes de anular a entrada
    unsigned long phys_frame = (unsigned long)pt[pt_idx].frames << 12;

    /*
     * LIBERTAÇÃO DA MEMÓRIA RAM FÍSICA
     * ------------------------------------------------------------------------
     * Devolve a página física recuperada da PT ao Physical Memory Manager (PMM).
     */
    pmm_free_page(phys_frame);

    // Desfaz o mapeamento na entrada da tabela de forma segura
    pt[pt_idx].p = 0;
    pt[pt_idx].frames = 0;

    // 5. Invalida imediatamente o cache TLB do processador para o endereço virtual alvo
    __asm__ __volatile__("invlpg (%0)" :: "r"(virt) : "memory");
}
