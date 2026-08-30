/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pmm.h
 *    Description: Protótipos e definições do Gestor de Memória Física (PMM).
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

#ifndef _PMM_H_
#define _PMM_H_

#include <kernel/boot_info.h>

#include "memory_map.h" // contem PAGE_SIZE

/*
 * Inicializa o Gestor de Memória Física com base no mapa do Bootloader.
 */
void pmm_init(BOOT_INFO *boot_info);

/*
 * Aloca uma única página de memória física livre.
 * Retorna o endereço físico da página ou 0 em caso de erro.
 */
unsigned long pmm_alloc_page(void);

/*
 * Liberta uma página de memória física previamente alocada.
 */
void pmm_free_page(unsigned long phys_address);


/*
 * Aloca uma sequência contígua de 'count' páginas de memória física.
 * Retorna o endereço físico do início do bloco ou 0 em caso de erro.
 */
unsigned long pmm_alloc_pages(unsigned long count);

/*
 * Liberta uma sequência contígua de 'count' página de memória física previamente alocada.
 */
void pmm_free_pages(unsigned long phys_address, unsigned long count);

#endif /* _PMM_H_ */
