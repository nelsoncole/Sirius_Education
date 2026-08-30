/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: heap.c
 *    Description: Implementação do alocador dinâmico do Kernel (Heap),
 *                 gerindo divisões de blocos em lista encadeada.
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

#include <kernel/kernel/mm/heap.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/arch/mm/paging.h>
#include <kernel/arch/mm/vmm.h>

// Ponteiro global que indica a raiz (início) da lista encadeada do Heap
static HEAP_HEADER* g_heap_start = (void*)0;

/*
 * INICIALIZAÇÃO DO HEAP DO KERNEL (KHEAP INIT)
 * ------------------------------------------------------------------------
 * Configura o cabeçalho primitivo sobre o espaço virtual de 2 MB alinhado.
 * Prepara a lista encadeada marcando todo o bloco inicial como livre.
 */
void kheap_init(void) {
    // Define a raiz do Heap no endereço virtual base configurado no memory_map.h
    g_heap_start = (HEAP_HEADER*)KERNEL_HEAP_VIRTUAL_BASE;

    // Configura os metadados do primeiro bloco gigante livre
    g_heap_start->size = KERNEL_HEAP_INITIAL_SIZE - sizeof(HEAP_HEADER);
    g_heap_start->is_free = 1;
    g_heap_start->next = (void*)0;
}

/*
 * ALOCAÇÃO DINÂMICA DE MEMÓRIA (KMALLOC - BEST-FIT)
 * ------------------------------------------------------------------------
 * Procura em toda a lista encadeada o bloco livre cujo tamanho seja o mais
 * próximo possível (melhor ajuste) do espaço solicitado pelo Kernel.
 * 
 * Nelson, outro algoritmo alternativo é o First-Fit.
 */
/*
 * LIMITES DINÂMICOS DO HEAP DO KERNEL
 * ------------------------------------------------------------------------
 * Variável de controle para rastrear o endereço virtual onde o Heap termina.
 * Permite saber o local exato para o VMM injetar novas páginas na expansão.
 */
static unsigned long g_heap_current_end = KERNEL_HEAP_VIRTUAL_BASE + KERNEL_HEAP_INITIAL_SIZE;

/*
 * ALOCAÇÃO DINÂMICA DE MEMÓRIA COM EXPANSÃO (KMALLOC - BEST-FIT)
 * ------------------------------------------------------------------------
 * Procura o bloco ideal. Se o espaço livre acabar, o sistema solicita
 * automaticamente novas páginas ao PMM e expande o espaço virtual via VMM.
 */
void* kmalloc(unsigned long size) {
    if (size == 0) return (void*)0;

    // Alinha o tamanho solicitado para 8 bytes para garantir performance de barramento
    size = (size + 7) & ~7UL;

    HEAP_HEADER* current;
    HEAP_HEADER* best_block;
    unsigned long smallest_diff;

    // Rótulo para repetição do ciclo após o Heap ser expandido com sucesso
    retry_search:

    current = g_heap_start;
    best_block = (void*)0;
    smallest_diff = 0xFFFFFFFFFFFFFFFFUL;

    // 1. Varre a lista completa para encontrar o bloco ideal (Best-Fit)
    while (current != (void*)0) {
        if (current->is_free && current->size >= size) {
            unsigned long diff = current->size - size;
            if (diff < smallest_diff) {
                smallest_diff = diff;
                best_block = current;
            }
        }
        current = current->next;
    }

    /*
     * 2. GESTÃO DE ESGOTAMENTO DE MEMÓRIA (EXPANSÃO DINÂMICA)
     * Se nenhum bloco livre foi encontrado, expandimos o Heap em mais 1 MB.
     */
    if (best_block == (void*)0) {
        unsigned long expansion_size = 1024 * 1024; // Expansão padrão de 1 MB
        PML4_TABLE* pml4 = (PML4_TABLE*)PML4_ADDRESS;

        // Garante que o tamanho da expansão consegue cobrir pelo menos a alocação atual
        if (size + sizeof(HEAP_HEADER) > expansion_size) {
            expansion_size = (size + sizeof(HEAP_HEADER) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        }

        /*
         * Mapeamento físico/virtual do novo bloco de extensão.
         * Fornece novas páginas do PMM e injeta-as de forma contínua.
         */
        for (unsigned long offset = 0; offset < expansion_size; offset += PAGE_SIZE) {
            unsigned long phys_page = pmm_alloc_page();
            
            // Se o PMM esgotar a RAM física do hardware, não há como expandir
            if (phys_page == 0) return (void*)0; 

            vmm_map_page(pml4, g_heap_current_end + offset, phys_page, 0x2);
        }

        // Configura o cabeçalho do novo espaço adicionado
        HEAP_HEADER* new_space_header = (HEAP_HEADER*)g_heap_current_end;
        new_space_header->size = expansion_size - sizeof(HEAP_HEADER);
        new_space_header->is_free = 1;
        new_space_header->next = (void*)0;

        // Atualiza o limite virtual superior do Heap do Kernel
        g_heap_current_end += expansion_size;

        /*
         * Anexa o novo bloco expandido no fim da lista encadeada existente
         */
        current = g_heap_start;
        while (current->next != (void*)0) {
            current = current->next;
        }
        current->next = new_space_header;

        /*
         * TÉCNICA DE COALESCING ANTECIPADO:
         * Se o último bloco da lista antiga já estava livre, funde-o imediatamente 
         * com o novo espaço criado para gerar um bloco livre contínuo e gigante.
         */
        if (current->is_free) {
            current->size += sizeof(HEAP_HEADER) + new_space_header->size;
            current->next = new_space_header->next;
        }

        // Reinicia a busca pelo algoritmo Best-Fit com o novo espaço disponível
        goto retry_search;
    }

    /*
     * 3. AVALIA A NECESSIDADE DE DIVISÃO (SPLIT) DO BLOCO
     */
    if (best_block->size >= (size + sizeof(HEAP_HEADER) + 8)) {
        unsigned long next_header_addr = (unsigned long)best_block + sizeof(HEAP_HEADER) + size;
        HEAP_HEADER* new_next_block = (HEAP_HEADER*)next_header_addr;

        new_next_block->size = best_block->size - size - sizeof(HEAP_HEADER);
        new_next_block->is_free = 1;
        new_next_block->next = best_block->next;

        best_block->size = size;
        best_block->next = new_next_block;
    }

    // Marca o bloco escolhido como ocupado
    best_block->is_free = 0;

    // Retorna o ponteiro virtual útil pronto para uso
    return (void*)((unsigned long)best_block + sizeof(HEAP_HEADER));
}


/*
 * LIBERTAÇÃO DE MEMÓRIA DINÂMICA (KFREE)
 * ------------------------------------------------------------------------
 * Devolve o bloco à lista de espaços livres e realiza a fusão contígua
 * (coalescing) de blocos livres vizinhos para mitigar a fragmentação.
 */
void kfree(void* ptr) {
    if (ptr == (void*)0) return;

    // Recupera o cabeçalho original recuando o tamanho dos metadados
    HEAP_HEADER* header = (HEAP_HEADER*)((unsigned long)ptr - sizeof(HEAP_HEADER));
    header->is_free = 1;

    /*
     * FUSÃO DE BLOCOS LIVRES CONSECUTIVOS (COALESCING)
     * Varre a lista a partir da raiz para unificar nós livres adjacentes.
     */
    HEAP_HEADER* current = g_heap_start;
    while (current != (void*)0) {
        // Se o bloco atual está livre e o próximo também está, funde-os
        if (current->is_free && current->next != (void*)0 && current->next->is_free) {
            current->size += sizeof(HEAP_HEADER) + current->next->size;
            current->next = current->next->next;
            
            // Não avança o ponteiro para reavaliar o novo bloco fundido com o seguinte
            continue; 
        }
        current = current->next;
    }
}
