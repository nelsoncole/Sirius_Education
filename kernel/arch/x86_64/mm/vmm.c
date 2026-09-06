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

#include <kernel/arch/x86_64/mm/vmm.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/kernel/core/panic.h>
#include <kernel/klib.h>


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
        
        unsigned long long* virt_ptr = (unsigned long long*)vmm_scratch_map_internal(new_table_phys);
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
    PAGE_DIRECTORY_POINTER_TABLE* pdpt = (PAGE_DIRECTORY_POINTER_TABLE*)vmm_scratch_map_internal(pdpt_phys);
    if (!pdpt[pdpt_idx].p) {
        unsigned long new_table_phys = pmm_alloc_page();
        
        unsigned long long* virt_ptr = (unsigned long long*)vmm_scratch_map_internal(new_table_phys);
        for (int i = 0; i < 512; i++) virt_ptr[i] = 0;
        
        // Reabre a PDPT para salvar o vínculo do novo frame físico
        pdpt = (PAGE_DIRECTORY_POINTER_TABLE*)vmm_scratch_map_internal(pdpt_phys);
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
    PAGE_DIRECTORY* pd = (PAGE_DIRECTORY*)vmm_scratch_map_internal(pd_phys);
    if (!pd[pd_idx].p) {
        unsigned long new_table_phys = pmm_alloc_page();
        
        unsigned long long* virt_ptr = (unsigned long long*)vmm_scratch_map_internal(new_table_phys);
        for (int i = 0; i < 512; i++) virt_ptr[i] = 0;
        
        // Reabre a PD para salvar o vínculo do novo frame físico da PT
        pd = (PAGE_DIRECTORY*)vmm_scratch_map_internal(pd_phys);
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
    PAGE_TABLE* pt = (PAGE_TABLE*)vmm_scratch_map_internal(pt_phys);
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
    PAGE_DIRECTORY_POINTER_TABLE* pdpt = (PAGE_DIRECTORY_POINTER_TABLE*)vmm_scratch_map_internal(pdpt_phys);
    if (!pdpt[pdpt_idx].p) return;
    unsigned long pd_phys = (unsigned long)pdpt[pdpt_idx].phy_addr_pd << 12;

    // 3. Verifica se a PT existe
    PAGE_DIRECTORY* pd = (PAGE_DIRECTORY*)vmm_scratch_map_internal(pd_phys);
    if (!pd[pd_idx].p) return;
    unsigned long pt_phys = (unsigned long)pd[pd_idx].phy_addr_pt << 12;

    // 4. Mapeia a PT final para inspecionar e modificar a entrada da página
    PAGE_TABLE* pt = (PAGE_TABLE*)vmm_scratch_map_internal(pt_phys);
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

/*
 * Mapeia com segurança qualquer dispositivo físico ou tabela de firmware (MMIO)
 * dentro da janela massiva de 512 GB (PML4 Índice 510).
 *
 * Parâmetros:
 *   phys_addr: O endereço físico real do dispositivo na placa-mãe.
 *   size:      O tamanho em bytes da região do dispositivo.
 * 
 * Retorna:
 *   O ponteiro virtual correspondente no Higher-Half pronto para leitura/escrita.
 */
void* vmm_map_device(unsigned long phys_addr, unsigned long size)
{
    PML4_TABLE* pml4 = (PML4_TABLE*)PML4_ADDRESS;
    if (size == 0) return 0;

    // 1. ISOLAR OFFSETS E ALINHAR AS PÁGINAS A FRONTEIRAS DE 4 KB
    // Captura o início exato da página física (zera os 12 bits inferiores)
    unsigned long phys_page   = phys_addr & ~(PAGE_SIZE - 1);
    // Captura o deslocamento do endereço dentro daquela página (0 a 4095)
    unsigned long page_offset = phys_addr & (PAGE_SIZE - 1);
    
    // Calcula a quantidade real de páginas de 4 KB que o tamanho total exige
    unsigned long num_pages = (size + page_offset + PAGE_SIZE - 1) / PAGE_SIZE;

    /*
     * 2. PONTEIRO LINEAR DE ALOCAÇÃO VIRTUAL
     * Usamos uma variável estática que retém o seu valor entre chamadas.
     * Ela inicia na base de 512 GB e "anda para a frente" a cada novo dispositivo,
     * garantindo que um hardware nunca sobreponha o espaço virtual do outro.
     */
    static unsigned long next_available_virtual = KERNEL_MMIO_VIRTUAL_BASE;
    unsigned long start_virtual_address = next_available_virtual;

    /*
     * 3. CONFIGURAÇÃO DE FLAGS DE HARDWARE (CACHE DISABLE)
     * Para registadores de IO (como LAPIC/IOAPIC), precisamos de desativar o cache.
     *   Bit 0 (0x01) -> Present
     *   Bit 1 (0x02) -> Read/Write
     *   Bit 3 (0x08) -> Page-level Write-Through (PWT)
     *   Bit 4 (0x10) -> Page-level Cache Disable (PCD)
     *   Total das flags por hardware = 0x1B
     */
    unsigned int mmio_flags = 0x1B;

    // 4. MAPEAR PÁGINA POR PÁGINA NA ÁRVORE DE PAGINAÇÃO
    for (unsigned long i = 0; i < num_pages; i++)
    {
        // Carimba a associação física-virtual na tabela CR3 do Kernel
        vmm_map_page(pml4, next_available_virtual, phys_page + (i * PAGE_SIZE), mmio_flags);
        
        // Desloca 4 KB para a frente na janela virtual de MMIO
        next_available_virtual += PAGE_SIZE;

        // Proteção estrita contra estouro do limite superior de 512 GB
        if (next_available_virtual >= KERNEL_MMIO_VIRTUAL_END)
        {
            kernel_panic("[VMM ERRO CRITICO] Espaco virtual massivo de 512 GB para MMIO esgotado!\n");
            for(;;);
        }
    }

    /*
     * 5. RETORNA O ENDEREÇO VIRTUAL AJUSTADO
     * Devolvemos o endereço de partida virtual somado ao offset original.
     * Desta forma, se o hardware começava no byte 512 da página física,
     * o ponteiro devolverá o byte 512 da página virtual gerada.
     */
    return (void *)(start_virtual_address + page_offset);
}

/**
 * Cria um novo espaço de endereçamento virtual (PML4).
 * Aloca a página raíz, limpa o espaço do utilizador e clona a metade do Kernel.
 * 
 * @return O endereço físico do novo PML4 (pronto para ser guardado no proc->cr3).
 */
unsigned long vmm_create_address_space(void)
{
    /* 1. Aloca uma página física para o novo PML4 (4096 bytes) */
    unsigned long new_pml4_phys = pmm_alloc_page(); 
    if (!new_pml4_phys) 
    {
        kprintf("[VMM] Erro: Falha ao alocar pagina fisica para o novo PML4.\n");
        return 0;
    }

    /* Captura o PML4 (CR3) ativo no Kernel atualmente */
    unsigned long current_pml4_phys;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(current_pml4_phys));
    
    /* 
     * CORREÇÃO CRÍTICA: Buffer puro de 64 bits (8 bytes por entrada).
     * Armazena os bits brutos das 256 entradas da metade superior do Kernel.
     */
    unsigned long long kernel_entries_buffer[256];

    /* Mapeia o PML4 do Kernel na janela temporária para leitura */
    unsigned long long* current_pml4_raw = (unsigned long long*)vmm_scratch_map_internal(current_pml4_phys);
    
    /* Salva com segurança a metade superior do Kernel no stack local */
    for (int i = 256; i < 512; i++) 
    {
        kernel_entries_buffer[i - 256] = current_pml4_raw[i];
    }

    /* 
     * 2. Mapeia a nova página PML4 alocada na Janela Temporária 
     * substituindo o mapeamento antigo.
     */
    unsigned long long* new_pml4_raw = (unsigned long long*)vmm_scratch_map_internal(new_pml4_phys);

    /* 3. Limpa a metade inferior (Índices 0 a 255 -> Espaço do Utilizador) */
    for (int i = 0; i < 256; i++) 
    {
        new_pml4_raw[i] = 0ULL;
    }

    /* 4. Restaura a metade superior clonada do Kernel (Índices 256 a 511) */
    for (int i = 256; i < 512; i++) 
    {
        new_pml4_raw[i] = kernel_entries_buffer[i - 256];
    }

    /* Retorna o endereço físico perfeitamente alinhado a 4KB */
    return (new_pml4_phys & ~0xFFFUL);
}