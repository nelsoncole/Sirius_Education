/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vmm.h
 *    Description: Protótipos e definições para o Gerenciador de Memória Virtual
 *                 (VMM) em arquitetura x86_64.
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

#ifndef _VMM_H_
#define _VMM_H_

#include "paging.h"

// Macros para extração de índices retiradas do conceito x86_64
#define GET_PML4_INDEX(virt) (((virt) >> 39) & 0x1FF)
#define GET_PDPT_INDEX(virt) (((virt) >> 30) & 0x1FF)
#define GET_PD_INDEX(virt)   (((virt) >> 21) & 0x1FF)
#define GET_PT_INDEX(virt)   (((virt) >> 12) & 0x1FF)

/*
 * CONFIGURAÇÃO E INICIALIZAÇÃO DO GERENCIADOR VIRTUAL (VMM SETUP)
 * ------------------------------------------------------------------------
 * Prepara o ambiente de paginação definitivo para o funcionamento do Kernel.
 * Configura a Janela Temporária (Scratch) e realiza o mapeamento primitivo
 * dos 2 MB regulamentares para o Heap estável do sistema operativo.
 */
void vmm_init(void);

// Mapeia um endereço virtual para um endereço físico com flags específicas
void vmm_map_page(PML4_TABLE* pml4, unsigned long virt, unsigned long phys, unsigned long flags);

// Remove o mapeamento de uma página virtual
void vmm_unmap_page(PML4_TABLE* pml4, unsigned long virt);

/*
 * OPERAÇÃO DE ALTERAÇÃO DO DIRETÓRIO RAÍZ
 * ------------------------------------------------------------------------
 * Atualiza o registo CR3 do processador utilizando o endereço físico bruto.
 * Força a MMU a transitar imediatamente para o novo espaço virtual.
 */
void vmm_switch_pml4(unsigned long pml4_phys);

/*
 * CONFIGURAÇÃO INICIAL DA JANELA TEMPORÁRIA (VMM SCRATCH WINDOW)
 * ------------------------------------------------------------------------
 * Prepara a infraestrutura de tabelas necessária para suportar a janela.
 * Garante a alocação da PT vinculada aos índices altos da MMU.
 */
void vmm_scratch_setup(void);

/*
 * OPERAÇÃO VOLÁTIL DE TROCA DE FRAME (SCRATCH MAP)
 * ------------------------------------------------------------------------
 * Substitui instantaneamente o frame físico mapeado na janela de 4 KB.
 * Invalida a cache TLB e retorna o ponteiro virtual fixo pronto a usar.
 */
void* vmm_scratch_map(unsigned long phys_addr);

#endif /* _VMM_H_ */

