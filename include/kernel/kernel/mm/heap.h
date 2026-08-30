/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: heap.h
 *    Description: Estruturas de controlo e protótipos para o alocador de
 *                 memória dinâmica do Kernel (Kernel Heap).
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

#ifndef _HEAP_H_
#define _HEAP_H_

#include "memory_map.h"

/*
 * CADEÇALHO DE BLOCO DO HEAP (HEAP HEADER)
 * ------------------------------------------------------------------------
 * Estrutura de metadados posicionada no início de cada bloco do Heap.
 * Permite rastrear o tamanho e o estado de ocupação na lista encadeada.
 */
typedef struct _HEAP_HEADER {
    unsigned long size;            // Tamanho útil do bloco (excluindo este cabeçalho)
    int is_free;                   // Flag: 1 se o bloco estiver livre, 0 se ocupado
    struct _HEAP_HEADER* next;     // Ponteiro para o próximo bloco na lista
} __attribute__((packed)) HEAP_HEADER;

/*
 * INICIALIZAÇÃO DO HEAP DO KERNEL
 * ------------------------------------------------------------------------
 * Configura o bloco inicial de 2 MB previamente mapeado pelo VMM.
 */
void kheap_init(void);

/*
 * ALOCAÇÃO DINÂMICA DE MEMÓRIA (KMALLOC)
 * ------------------------------------------------------------------------
 * Reserva um bloco contíguo de memória virtual no Heap do Kernel.
 */
void* kmalloc(unsigned long size);

/*
 * LIBERTAÇÃO DE MEMÓRIA DINÂMICA (KFREE)
 * ------------------------------------------------------------------------
 * Devolve um bloco previamente alocado à lista de espaços livres do Heap.
 */
void kfree(void* ptr);

#endif /* _HEAP_H_ */