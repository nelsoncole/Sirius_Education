/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: uheap.h
 *    Description: Cabeçalho do Gestor de Heap do Espaço de Utilizador (Ring 3)
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

#ifndef _UHEAP_H_
#define _UHEAP_H_

#include <stddef.h>
#include <stdint.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

/* Cabeçalho alinhado a 16 bytes (32 bytes no total) */
typedef struct _UHEAP_HEADER {
    uint64_t size;                 // Tamanho útil do bloco
    uint64_t is_free;              // 1 se livre, 0 se ocupado
    struct _UHEAP_HEADER* next;    // Ponteiro para o próximo bloco
    uint64_t padding;              // Garante alinhamento geométrico de 16 bytes
} __attribute__((packed)) UHEAP_HEADER;

/* Assinaturas das funções expostas para o Ring 3 */
void* umalloc(size_t size);
void  ufree(void* ptr);
void* ucalloc(size_t num, size_t size);
void* urealloc(void* ptr, size_t new_size);

#endif /* _UHEAP_H_ */