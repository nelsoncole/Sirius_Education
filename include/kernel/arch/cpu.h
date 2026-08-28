/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: cpu.h
 *    Description: Centraliza tudo o que o kernel precisa de saber 
 *                 sobre o estado local de cada CPU.
 * 
 *         Author: Nelson Cole
 *   Created Date: 28/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 28/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef __CPU_H__
#define __CPU_H__

#include <kernel/stdint.h>

#include "gdt.h"
#include "tss.h"
#include "idt.h"

#define MAX_CPUS 256

// O teu bloco de dados local por CPU
typedef struct {
    
    uint64_t gdt_entries[8] __attribute__((aligned(8)));
    gdtr_t gdtr;
    tss_t tss __attribute__((aligned(16)));

    uint32_t lapic_id;
    uint32_t cpu_id;
    uint64_t kernel_stack_top;
} __attribute__((packed)) cpu_data_block_t;

// Array global de ponteiros para que qualquer parte do kernel possa inspecionar outros cores
extern cpu_data_block_t* cpu_blocks[MAX_CPUS];

// Função para inicializar o bloco do core atual
void cpu_initialize_local(uint32_t cpu_id, uint32_t lapic_id, uint64_t stack_top);

#endif