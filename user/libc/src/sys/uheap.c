/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: uheap.c
 *    Description: Gestor de Heap do Espaço de Utilizador (Ring 3) em arquivo único.
 *                 Reaproveita o algoritmo Best-Fit, Split e Coalescing do Kernel,
 *                 utilizando a sys_brk nativa via funções static inline.
 *
 *         Author: Nelson Cole
 *   Created Date: 17/09/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 25/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

// Consome as macros SYS_BRK e as funções inline syscallX
#include <sys/usyscall.h>
#include <sys/uheap.h>
#include <stdio.h>

#ifndef null
#define null ((void *)0)
#endif

// Ponteiros estáticos de controlo do Heap local do Processo
static UHEAP_HEADER* g_uheap_start = null;
uint64_t g_uheap_current_end = 0;

/* Funções auxiliares */
static inline void* u_memset(void* dest, int val, size_t len) {
    unsigned char* ptr = (unsigned char*)dest;
    while (len-- > 0) *ptr++ = (unsigned char)val;
    return dest;
}

static inline void* u_memcpy(void* dest, const void* src, size_t len) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (len-- > 0) *d++ = *s++;
    return dest;
}

/**
 * Invólucro que interage com a Syscall BRK do Kernel
 */
static inline void* sbrk_user(intptr_t increment) 
{
    // 1. Descobre o break atual consultando o Kernel via usyscall.h
    uint64_t current_break = syscall1(SYS_BRK, 0);
    if (increment == 0) return (void*)current_break;

    // 2. Solicita o novo limite expandido ao Kernel
    uint64_t new_break = current_break + increment;
    uint64_t result = syscall1(SYS_BRK, new_break);

    if (result == current_break) return (void*)-1; // Out of Memory em Ring 3

    return (void*)current_break;
}

/**
 * @brief Aloca um bloco de memória alinhado a 16 bytes no Heap do Utilizador.
 */
void* umalloc(size_t size) 
{
    if (size == 0) return null;

    // Alinhamento estrito a 16 bytes para os dados do utilizador
    size = (size + 15) & ~15UL;

    UHEAP_HEADER* current;
    UHEAP_HEADER* best_block = null;
    size_t smallest_diff = 0xFFFFFFFFFFFFFFFFUL;
    
    // Como sizeof(UHEAP_HEADER) é 32, a constante é limpa e segura
    const size_t h_size = sizeof(UHEAP_HEADER); 

    // 1. Inicialização tardia do Heap
    if (g_uheap_start == null) 
    {
        uint64_t base_break = (uint64_t)sbrk_user(0);
        if (base_break == (uint64_t)-1) return null;

        unsigned long initial_expansion = 16 * 1024;
        if(size > (initial_expansion - h_size)) 
            initial_expansion = (size + h_size + 15) & ~15UL;

        void* first_space = sbrk_user(initial_expansion);
        if (first_space == (void*)-1) return null;

        g_uheap_start = (UHEAP_HEADER*)first_space;
        g_uheap_start->size = initial_expansion - h_size;
        g_uheap_start->is_free = 1;
        g_uheap_start->next = null;

        g_uheap_current_end = (uint64_t)first_space + initial_expansion;
    }

    // 2. Busca Best-Fit
    current = g_uheap_start;
    while (current != null) 
    {
        if (current->is_free && current->size >= size) 
        {
            size_t diff = current->size - size;
            if (diff < smallest_diff) 
            {
                smallest_diff = diff;
                best_block = current;
            }
        }
        current = current->next;
    }

    // 3. Expansão do break se não encontrou bloco
    if (best_block == null) 
    {
        unsigned long expansion_size = 16 * 1024;
        if (size + h_size > expansion_size) 
        {
            expansion_size = (size + h_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        }

        void* allocated_space = sbrk_user(expansion_size);
        if (allocated_space == (void*)-1) return null;

        best_block = (UHEAP_HEADER*)allocated_space;
        best_block->size = expansion_size - h_size;
        best_block->is_free = 1;
        best_block->next = null;

        g_uheap_current_end = (uint64_t)allocated_space + expansion_size;

        // Encontra o último nó atual da lista
        current = g_uheap_start;
        while (current->next != null) 
        {
            current = current->next;
        }
        
        // CORREÇÃO: Liga de forma segura o novo bloco ao fim da cadeia
        current->next = best_block;

        // Fusão segura com o anterior se este estiver livre
        if (current->is_free) 
        {
            current->size += h_size + best_block->size;
            current->next = best_block->next; // Mantém a coerência da cauda da lista (NULL neste caso)
            best_block = current; 
        }
    }

    // 4. Split (Divisão) - Totalmente alinhado porque h_size(32) + size(Múltiplo de 16) é perfeitamente alinhado
    if (best_block->size >= (size + h_size + 16)) 
    {
        unsigned long next_header_addr = (unsigned long)best_block + h_size + size;
        UHEAP_HEADER* new_next_block = (UHEAP_HEADER*)next_header_addr;

        new_next_block->size = best_block->size - size - h_size;
        new_next_block->is_free = 1;
        new_next_block->next = best_block->next;

        best_block->size = size;
        best_block->next = new_next_block;
    }

    best_block->is_free = 0;
    return (void*)((unsigned long)best_block + h_size);
}

/**
 * @brief Liberta e funde blocos de memória dinâmica em Ring 3.
 */
void ufree(void* ptr) 
{
    if (ptr == null) return;

    // Recua 32 bytes para apanhar as propriedades físicas do cabeçalho
    UHEAP_HEADER* header = (UHEAP_HEADER*)((unsigned long)ptr - sizeof(UHEAP_HEADER));
    header->is_free = 1;

    // COALESCING COMPLETO (Funde fragmentos órfãos consecutivamente)
    UHEAP_HEADER* current = g_uheap_start;
    while (current != null) 
    {
        if (current->is_free && current->next != null && current->next->is_free) 
        {
            current->size += sizeof(UHEAP_HEADER) + current->next->size;
            current->next = current->next->next;
            continue; 
        }
        current = current->next;
    }
}



/**
 * @brief Aloca memória para um array de elementos e zera todos os bytes.
 * @param num Número de elementos.
 * @param size Tamanho de cada elemento.
 */
void* ucalloc(size_t num, size_t size) 
{
    size_t total_size = num * size;
    
    // Aloca o bloco via umalloc
    void* ptr = umalloc(total_size);
    if (ptr == null) return null;

    // Garante que o espaço vem totalmente limpo e zerado
    u_memset(ptr, 0, total_size);
    return ptr;
}

/**
 * @brief Realoca um bloco de memória, expandindo ou contraindo o seu espaço útil.
 * @param ptr Ponteiro antigo alocado.
 * @param new_size Novo tamanho desejado.
 */
void* urealloc(void* ptr, size_t new_size) 
{
    // Se o ponteiro for nulo, comporta-se exatamente como um umalloc regular
    if (ptr == null) return umalloc(new_size);

    // Se o tamanho for zero, liberta o espaço e retorna nulo
    if (new_size == 0) 
    {
        ufree(ptr);
        return null;
    }

    // Pega no cabeçalho para saber o tamanho útil atual do bloco
    UHEAP_HEADER* header = (UHEAP_HEADER*)((unsigned long)ptr - sizeof(UHEAP_HEADER));
    size_t old_size = header->size;

    // Se o novo tamanho for menor ou igual ao atual, podemos reutilizar o mesmo bloco!
    // (Opcionalmente poderia fazer split aqui, mas manter o bloco poupa fragmentação)
    if (new_size <= old_size) 
    {
        return ptr;
    }

    // Aloca um novo bloco contíguo com o tamanho expandido solicitado
    void* new_ptr = umalloc(new_size);
    if (new_ptr == null) return null; // Preserva o bloco antigo intacto se falhar

    // Copia cirurgicamente os dados antigos para o novo endereço conquistado
    u_memcpy(new_ptr, ptr, old_size);

    // Devolve o bloco de memória antigo para a lista de blocos livres do processo
    ufree(ptr);

    return new_ptr;
}