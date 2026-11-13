/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: mtrr.h
 *    Description: Definições, constantes e protótipos para gestão dos
 *                 Memory Type Range Registers (MTRR) em sistemas x86/x86_64
 *                 com suporte a inicialização BSP e sincronização AP (SMP).
 * 
 *         Author: Nelson Cole
 *   Created Date: 12/11/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 12/11/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _MTRR_H_
#define _MTRR_H_

#include <kernel/klib.h>

/* ============================================================================
 * MSRs de Controlo e Capacidade MTRR
 * ============================================================================ */
#define IA32_MTRR_CAP               0xFE
#define IA32_MTRR_DEF_TYPE          0x2FF

/* MSRs dos MTRRs Variáveis (Base e Máscara para o índice n) */
#define IA32_MTRR_PHYSBASE(n)       (0x200 + 2 * (n))
#define IA32_MTRR_PHYSMASK(n)       (0x201 + 2 * (n))

/* MSRs dos MTRRs Fixos (64KB, 16KB e 4KB) */
#define IA32_MTRR_FIX64K_00000      0x250
#define IA32_MTRR_FIX16K_80000      0x258
#define IA32_MTRR_FIX16K_A0000      0x259
#define IA32_MTRR_FIX4K_C0000       0x268
#define IA32_MTRR_FIX4K_C8000       0x269
#define IA32_MTRR_FIX4K_D0000       0x26A
#define IA32_MTRR_FIX4K_D8000       0x26B
#define IA32_MTRR_FIX4K_E0000       0x26C
#define IA32_MTRR_FIX4K_E8000       0x26D
#define IA32_MTRR_FIX4K_F0000       0x26E
#define IA32_MTRR_FIX4K_F8000       0x26F

/* ============================================================================
 * Bits de Controlo e Flags
 * ============================================================================ */
#define MTRR_ENABLE                 (1ULL << 11)
#define MTRR_FIXED_ENABLE           (1ULL << 10)
#define MTRR_PHYS_MASK_VALID        (1ULL << 11)

#define MTRR_CAP_VCNT_MASK          0xFFULL
#define MTRR_CAP_FIX_SUPPORTED      (1ULL << 8)
#define MTRR_CAP_WC_SUPPORTED       (1ULL << 10)
#define MTRR_CAP_SMRR_SUPPORTED     (1ULL << 11)

/* ============================================================================
 * Tipos de Memória MTRR
 * ============================================================================ */
#define MTRR_TYPE_UNCACHEABLE       0x00 /* UC - Sem cache (MMIO / Dispositivos) */
#define MTRR_TYPE_WRITECOMBINING    0x01 /* WC - Write-Combining (Framebuffer) */
#define MTRR_TYPE_WRITETHROUGH      0x04 /* WT - Write-Through */
#define MTRR_TYPE_WRITEPROTECT      0x05 /* WP - Write-Protected */
#define MTRR_TYPE_WRITEBACK         0x06 /* WB - Write-Back (RAM Principal) */

/* ============================================================================
 * Limites e Códigos de Erro
 * ============================================================================ */
#define MAX_VARIABLE_MTRRS          16

#define MTRR_SUCCESS                 0
#define MTRR_ERR_NOT_SUPPORTED      -1
#define MTRR_ERR_INVALID_SIZE       -2
#define MTRR_ERR_UNALIGNED_BASE     -3
#define MTRR_ERR_WC_NOT_SUPPORTED   -4
#define MTRR_ERR_NO_FREE_SLOT       -5

/* ============================================================================
 * Estruturas de Dados
 * ============================================================================ */

/**
 * @brief Informação global sobre as capacidades MTRR do processador.
 */
typedef struct {
    int supported;              /* 1 se MSR e MTRR forem suportados, 0 caso contrário */
    uint32_t variable_count;    /* Número de pares de registos MTRR variáveis */
    int fixed_supported;        /* 1 se MTRRs fixos forem suportados */
    int wc_supported;           /* 1 se Write-Combining for suportado */
    uint64_t phys_addr_mask;    /* Máscara de bits físicos válidos (MAXPHYADDR) */
} mtrr_info_t;

/**
 * @brief Entrada guardada para replicação e sincronização entre núcleos (SMP).
 */
typedef struct {
    uint64_t base;              /* Valor completo do registo IA32_MTRR_PHYSBASE */
    uint64_t mask;              /* Valor completo do registo IA32_MTRR_PHYSMASK */
    int active;                 /* 1 se o registo estiver válido/ativo */
} mtrr_saved_entry_t;

/* ============================================================================
 * Protótipos de Funções
 * ============================================================================ */

/**
 * @brief Verifica via CPUID se a CPU suporta MSRs e MTRRs.
 * @return 1 se suportado, 0 caso contrário.
 */
int cpu_supports_mtrr(void);

/**
 * @brief Inicializa o subsistema MTRR no núcleo atual (uso interno).
 */
void mtrr_init(void);

/**
 * @brief Inicializa os MTRRs no Bootstrap Processor (BSP) e guarda o mapa base.
 */
void mtrr_init_bsp(void);

/**
 * @brief Sincroniza os MTRRs de um Application Processor (AP) com o estado do BSP.
 */
void mtrr_init_ap(void);

/**
 * @brief Configura um registo MTRR variável específico.
 * @param index Índice do registo variável (0 a variable_count - 1).
 * @param base  Endereço físico base (alinhado ao tamanho).
 * @param size  Tamanho da região em bytes (potência de 2, mínimo 4KB).
 * @param type  Tipo de memória (ex: MTRR_TYPE_WRITECOMBINING).
 * @return MTRR_SUCCESS (0) ou código de erro negativo.
 */
int mtrr_set_variable(uint32_t index, uint64_t base, uint64_t size, uint8_t type);

/**
 * @brief Procura automaticamente um slot livre e configura uma região MTRR.
 * @param base Endereço físico base (alinhado ao tamanho).
 * @param size Tamanho da região em bytes (potência de 2, mínimo 4KB).
 * @param type Tipo de memória (ex: MTRR_TYPE_WRITECOMBINING).
 * @return Índice do slot utilizado (>= 0) ou código de erro negativo.
 */
int mtrr_add_region(uint64_t base, uint64_t size, uint8_t type);

/**
 * @brief Desativa e limpa um registo MTRR variável específico.
 * @param index Índice do registo variável a limpar.
 */
void mtrr_clear_variable(uint32_t index);

/**
 * @brief Obtém um ponteiro constante para a estrutura de estado MTRR.
 * @return Ponteiro para mtrr_info_t.
 */
const mtrr_info_t *mtrr_get_info(void);


#endif /* _MTRR_H_ */
