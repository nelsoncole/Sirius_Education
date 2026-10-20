/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: shm.h
 *    Description: Implementação do Alocador Pool para memória compartilhada.
 *                 Define os limites da janela de memória virtual e exporta
 *                 as funções de gerenciamento para drivers e E/S.
 * 
 *         Author: Nelson Cole
 *   Created Date: 18/10/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 18/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _SHM_H_
#define _SHM_H_

#include <kernel/lib/stddef.h>


void shm_init(void);

void* shm_alloc(size_t size, int flag);

void shm_free(void* virt_addr, size_t size);

unsigned long shm_virtual_to_physical(void* virt_addr);

#endif
