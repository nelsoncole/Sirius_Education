/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kernel.h
 *    Description: Cabeçalho global do Kernel. Centraliza os tipos base,
 *                 estruturas de boot e funções vitais da biblioteca interna.
 *
 *         Author: Nelson Cole
 *   Created Date: 27/08/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 29/08/2026
 *
 *        License: MIT
 * ============================================================================
 */

#ifndef _KERNEL_H_
#define _KERNEL_H_

/* ============================================================================
 * 1. CONFIGURAÇÕES & TIPOS PADRÃO
 * ============================================================================ */
// Se criares um tipos.h (com uint64_t, size_t, etc.), inclui-o aqui:
// #include <kernel/types.h>

/* ============================================================================
 * 2. ESTRUTURAS DE INICIALIZAÇÃO E BOOT
 * ============================================================================ */
#include <kernel/boot_info.h>

/* ============================================================================
 * 3. BIBLIOTECA INTERNA DO KERNEL (libk)
 * ============================================================================ */
// Mapeados em conformidade com as tuas pastas include/ e src/lib/
// #include <kernel/kprintf.h>
// #include <kernel/string.h>

/* ============================================================================
 * 4. FUNÇÕES GLOBAIS DO CORE
 * ============================================================================ */
// Ponto de entrada independente de arquitetura (chamado por src/arch/x86_64/boot/)
void kernel_main(BOOT_INFO *boot_info);

// Função de pânico do sistema (geralmente em src/kernel/core/panic.c)
// void panic(const char *message);

/* ============================================================================
 * 4. FUNÇÕES GLOBAIS DO ARCH MM
 * ============================================================================ */
// Ponto de entrada independente de arquitetura (chamado por src/arch/x86_64/mm/
void setup_paging(BOOT_INFO *boot_info);
unsigned long paging_map_region_bitmap( BOOT_INFO *boot_info, 
    unsigned long bitmap_phys_addr,
    unsigned long ram_size_bytes);

#endif // __KERNEL_H__
