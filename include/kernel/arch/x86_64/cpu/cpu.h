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
 *  Modified Date: 04/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _CPU_H_
#define _CPU_H_

#include <kernel/lib/stdint.h>

#include "gdt.h"
#include "tss.h"
#include "idt.h"

#define MAX_CPUS    256
// 5 entradas normais (Null, KCode, KData, UCode, UData) + 1 TSS (ocupa 2 slots de 8 bytes) = 7
#define GDT_ENTRIES 7 
/*
 * ============================================================================
 * PER-CPU DATA BLOCK (Alinhamento de 16 bytes forçado para estabilidade física)
 * ============================================================================
 */
typedef struct __attribute__((aligned(16))) {
    // Array interno da GDT local por Core (Alinhado a 16 bytes)
    uint64_t gdt_entries[GDT_ENTRIES] __attribute__((aligned(16)));
    
    // Descritor da GDT
    gdtr_t gdtr;
    
    // Estrutura TSS local por Core (Alinhada a 16 bytes)
    tss_t tss __attribute__((aligned(16)));

    // Identificadores de arquitetura do núcleo
    uint32_t lapic_id;
    uint32_t cpu_id;
    
    // Topo da pilha do Kernel para este processador
    uint64_t kernel_stack_top;
    
    // Auto-ponteiro: Permite ler a base da estrutura via instrução asm (mov %gs:offset, %reg)
    void *self; 
} cpu_data_block_t;

/*
 * ============================================================================
 * GLOBAL REGISTRY & INTERFACES
 * ============================================================================
 */

// Array global de ponteiros para que qualquer parte do kernel possa inspecionar outros cores
extern cpu_data_block_t* cpu_blocks[MAX_CPUS];

/**
 * @brief Inicializa o bloco de dados local (GDT, TSS, MSR GS_BASE) do core atual.
 * 
 * @param cpu_id O identificador lógico atribuído pelo Kernel a este Core.
 * @param lapic_id O ID do Local APIC retornado pelo hardware/MADT (ACPI).
 * @param stack_top O endereço virtual do topo da pilha de execução deste Core no Kernel.
 */
void cpu_initialize_local(uint32_t cpu_id, uint32_t lapic_id, uint64_t stack_top);

/**
 * @brief Obtém o bloco de dados Per-CPU do núcleo que está a executar o código.
 * @return Ponteiro de 64 bits para a estrutura cpu_data_block_t local.
 */
static inline cpu_data_block_t* get_current_cpu(void) {
    cpu_data_block_t *current_cpu = (void*)0;
    
    __asm__ __volatile__(
        "movq %%gs:%1, %0" 
        : "=r"(current_cpu) 
        : "m"(((cpu_data_block_t*)0)->self)
        : "memory"
    );
    
    return current_cpu;
}

/**
 * @brief Obtém diretamente o ID lógico do CPU atual através do registador GS.
 * @return O identificador do núcleo (0 para o BSP, 1+ para os APs).
 */
static inline uint32_t get_current_cpu_id(void) {
    uint32_t current_cpu_id = 0;
    
    __asm__ __volatile__(
        "movl %%gs:%1, %k0"
        : "=r"(current_cpu_id)
        : "m"(((cpu_data_block_t*)0)->cpu_id)
    );
    
    return current_cpu_id;
}

/**
 * @brief Thread Idle do Kernel. 
 *        Executada por cada CPU quando não existem tarefas prontas na fila.
 */
void cpu_idle(void);

#endif /* _CPU_H_ */
