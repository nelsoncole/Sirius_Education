/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: memory_map.h
 *    Description: Mapa de endereçamento virtual global do Kernel e Aplicativos,
 *                 definindo as bases para o PMM, Vídeo, Heaps e Pilhas em x86_64.
 * 
 *         Author: Nelson Cole
 *   Created Date: 30/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 05/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _MEMORY_MAP_H_
#define _MEMORY_MAP_H telescope

/* ========================================================================
 * CONFIGURAÇÕES GERAIS DE ARQUITETURA (x86_64)
 * ======================================================================== */
#define PAGE_SIZE                   0x1000UL        // Tamanho padrão da página (4 KB)


/* ========================================================================
 * MAPA DE ENDEREÇAMENTO VIRTUAL DO ESPAÇO DE UTILIZADOR (USER SPACE)
 * ======================================================================== */
/*
 * BASE DE CÓDIGO E DADOS DO APLICATIVO (.TEXT / .DATA)
 * ------------------------------------------------------------------------
 * Ponto de entrada virtual padrão para o carregamento de binários executáveis.
 * Posicionado no primeiro bloco limpo de 4 MB para evitar colisões com nulos.
 */
#define USER_CODE_VIRTUAL_BASE      0x0000000000400000UL

/*
 * HEAP DO APLICATIVO (DYN MEMORY)
 * ------------------------------------------------------------------------
 * Espaço inicial alocado para expansão dinâmica em Ring 3 (via malloc/sys_brk).
 * Posicionado acima da área de código, crescendo para cima.
 */
#define USER_HEAP_VIRTUAL_BASE      0x0000000008000000UL

/*
 * MEMÓRIA PARTILHADA DO UTILIZADOR (USER SHARED MEMORY BASE)
 * ------------------------------------------------------------------------
 * Ponto de entrada virtual para o mapeamento de segmentos IPC (shm).
 * Posicionado estrategicamente a meio do espaço canónico inferior, 
 * garantindo isolamento total contra a expansão ascendente do Heap 
 * e protegendo a descida dinâmica da Pilha (Stack) do utilizador.
 */
#define USER_SHM_VIRTUAL_BASE       0x0000700000000000UL

/*
 * TOPO DA PILHA DO UTILIZADOR (USER STACK TOP)
 * ------------------------------------------------------------------------
 * Base de inicialização da pilha privada em Ring 3 para variáveis locais.
 * Posicionada no limite superior estável da metade canónica inferior, 
 * crescendo para baixo em direção ao limite de segurança.
 */
#define USER_STACK_VIRTUAL_TOP      0x00007FFFFFFFF000UL
#define USER_STACK_INITIAL_SIZE     0x4000UL        // Tamanho inicial padrão (16 KB / 4 Página)


/* ========================================================================
 * MAPA DE ENDEREÇAMENTO VIRTUAL DO KERNEL
 * ======================================================================== */
/*
 * BASE PRINCIPAL DO KERNEL
 * ------------------------------------------------------------------------
 * Mapeado no índice 511 da PML4, Índice 510 da PDPT, Índice 0 da PD e Índice 0 da PT.
 * Garante o positioning estável do código e dados nativos do sistema.
 */
#define KERNEL_VIRTUAL_BASE         0xFFFFFFFF80000000UL

/*
 * ENDEREÇO VIRTUAL DO TRAMPOLIM DO SMP
 * ------------------------------------------------------------------------
 * Posicionado exatamente 3 MB acima da base do Kernel.
 * Espaço reservado e seguro dentro do primeiro gigabyte (-mcmodel=kernel)
 * para projetar o buffer de inicialização dos 256 APs.
 */
#define KERNEL_TRAMPOLINE_VIRTUAL_BASE (KERNEL_VIRTUAL_BASE + 0x300000UL) // 0xFFFFFFFF80300000UL

/*
 * ========================================================================
 * SUBSISTEMA DE JANELAS TEMPORÁRIAS DO VMM (COMPLETAMENTE ISOLADO)
 * ========================================================================
 * Alocado na região do Kernel Base no endereço virtual 0xFFFFFFFF803F0000UL.
 * Mapeado no índice 511 da PML4, Índice 510 da PDPT, Índice 1 da PD e Índice 496 da PT.
 * Ocupa os slots sequenciais 496, 497 e 498 da mesma Tabela de Páginas (PT).
 */
#define VMM_SCRATCH_ISOLATED_BASE    0xFFFFFFFF803F0000UL

// Mapeamento linear direto dos slots contíguos na PT vinculada ao Índice 1 da PD
#define VMM_SCRATCH_WINDOW           (VMM_SCRATCH_ISOLATED_BASE)        // Slot / PT Índice 496 (Legado)
#define VMM_SCRATCH_WINDOW_0         (VMM_SCRATCH_WINDOW + PAGE_SIZE)   // Slot / PT Índice 497 (Janela 0)
#define VMM_SCRATCH_WINDOW_1         (VMM_SCRATCH_WINDOW_0 + PAGE_SIZE) // Slot / PT Índice 498 (Janela 1)


/*
 * DISPOSITIVOS DE HARDWARE E MMIO GLOBAL (ACPI, LAPIC, IOAPIC, PCI)
 * ------------------------------------------------------------------------
 * Mapeado no índice 510 da PML4.
 * Cria uma janela virtual massiva de 512 GB para mapear qualquer dispositivo 
 * de hardware ou tabela de firmware sem risco de colisão.
 */
#define KERNEL_MMIO_VIRTUAL_BASE    0xFFFFFF0000000000UL
#define KERNEL_MMIO_VIRTUAL_END     0xFFFFFF8000000000UL

/*
 * BITMAP DE MEMÓRIA FÍSICA (PMM)
 * ------------------------------------------------------------------------
 * Mapeado no índice 256 da PML4, Índice 0 da PDPT, Índice 0 da PD e Índice 0 da PT.
 * Espaço útil isolado de 1 GB, ideal para acomodar os 16 MB necessários
 * para gerir até 512 GB de memória RAM física.
 */
#define KERNEL_BITMAP_VIRTUAL_BASE  0xFFFF800000000000UL


/*
 * ALLOC_POOL 
 * ----------------------------------------------------------------------------------------------
 * Regiao de memoria para uso geral do kernel como ex: memoria alocada para read().
 * Mapeado no índice 256 da PML4, Índice 0 da PDPT, Índice 128 da PD e Índice 0 da PT.
 */
#define KERNEL_POOL_VIRTUAL_BASE 0xFFFF800010000000ULL // Mapeia a janela de buffers dinâmicos
#define KERNEL_POOL_MAX_PAGES    131072                // Janela de 512 MB 


/*
 * HEAP DO KERNEL
 * ------------------------------------------------------------------------
 * Mapeado no índice 256 da PML4, Índice 1 da PDPT, Índice 0 da PD e Índice 0 da PT.
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