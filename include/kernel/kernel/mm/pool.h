/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pool.h
 *    Description: Interface pública do Alocador Pool do Kernel (VMM).
 *                 Define os limites da janela de memória virtual e exporta
 *                 as funções de gerenciamento para drivers e E/S.
 * 
 *         Author: Nelson Cole
 *   Created Date: 09/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 09/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _POOL_H_
#define _POOL_H_

#include <kernel/lib/stddef.h>

/**
 * @brief Inicializa o subsistema de Pool do Kernel zerando o bitmap de controlo.
 */
void pool_init(void);

/**
 * @brief Aloca um bloco contíguo de páginas físicas e mapeia-as na região da Pool.
 * @param size Tamanho total em bytes requisitado (será arredondado para múltiplas páginas).
 * @return     Ponteiro virtual utilizável pela CPU, ou NULL em caso de falha.
 */
void* pool_alloc(size_t size);

/**
 * @brief Liberta a memória física e o espaço de endereçamento virtual alocados pela Pool.
 * @param virt_addr Ponteiro virtual retornado previamente por alloc_pool.
 * @param size      Tamanho em bytes fornecido originalmente na alocação.
 */
void pool_free(void* virt_addr, size_t size);

/**
 * @brief Resolve o endereço físico correspondente ao início do buffer virtual da Pool.
 * @param virt_addr Ponteiro virtual de um buffer ativo da Pool.
 * @return          Endereço físico bruto (Frame base de 64 bits) pronto para o DMA AHCI.
 */
unsigned long pool_virtual_to_physical(void* virt_addr);

#endif /* _POOL_H_ */
