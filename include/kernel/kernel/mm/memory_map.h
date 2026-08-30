/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: memory_map.h
 *    Description: Mapa de endereçamento virtual global do Kernel, definindo
 *                 as bases para o PMM (Bitmap), Vídeo e Heap em x86_64.
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

#ifndef _MEMORY_MAP_H_
#define _MEMORY_MAP_H_

/* ========================================================================
 * CONFIGURAÇÕES GERAIS DE ARQUITETURA (x86_64)
 * ======================================================================== */
#define PAGE_SIZE                   0x1000UL        // Tamanho padrão da página (4 KB)

/* ========================================================================
 * MAPA DE ENDEREÇAMENTO VIRTUAL DO KERNEL
 * ======================================================================== */

/*
 * BASE PRINCIPAL DO KERNEL
 * ------------------------------------------------------------------------
 * Mapeado no índice 511 da PML4, Índice 510 da PDPT, Índice 0 da PD e Índice 0 da PT.
 * Garante o posicionamento estável do código e dados nativos do sistema.
 */
#define KERNEL_VIRTUAL_BASE         0xFFFFFFFF80000000UL

/*
 * JANELA TEMPORÁRIA DO VMM (SCRATCH WINDOW)
 * ------------------------------------------------------------------------
 * Mapeado no índice 511 da PML4, Índice 510 da PDPT, Índice 1 da PD e Índice 511 da PT.
 * Endereço virtual isolado para manipulação volátil de tabelas físicas.
 */
#define VMM_SCRATCH_WINDOW          0xFFFFFFFF803FF000UL

/*
 * BITMAP DE MEMÓRIA FÍSICA (PMM)
 * ------------------------------------------------------------------------
 * Mapeado no índice 256 da PML4, Índice 0 da PDPT, Índice 0 da PD e Índice 0 da PT.
 * Espaço útil isolado de 1 GB, ideal para acomodar os 16 MB necessários
 * para gerir até 512 GB de memória RAM física.
 */
#define KERNEL_BITMAP_VIRTUAL_BASE  0xFFFF800000000000UL

/*
 * HEAP DO KERNEL
 * ------------------------------------------------------------------------
 * Mapeado no índice 256 da PML4, Índice 0 da PDPT, Índice 512 da PD e Índice 0 da PT.
 * Espaço de memória virtual reservado para alocações dinâmicas (kmalloc).
 */
#define KERNEL_HEAP_VIRTUAL_BASE    0xFFFF800040000000UL
#define KERNEL_HEAP_INITIAL_SIZE    (2 * 1024 * 1024)   // Tamanho inicial de 2 MB (Preenche 1 PT inteira)

/*
 * FRAMEBUFFER DE VÍDEO (GOP / VBE)
 * ------------------------------------------------------------------------
 * Mapeado no índice 256 da PML4, Índice 3 da PDPT, Índice 0 da PD e Índice 0 da PT.
 * Garante acesso direto à memória gráfica a partir do espaço do Kernel.
 */
#define KERNEL_VIDEO_VIRTUAL_BASE   0xFFFF8000E0000000UL

#endif /* _MEMORY_MAP_H_ */
