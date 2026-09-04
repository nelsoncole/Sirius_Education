/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: cpu.c
 *    Description: Implementação da gestão estruturada por núcleo (Per-CPU)
 *                 com suporte a GDT, MSR e TSS alinhados para Hardware Real.
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

#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/kernel/mm/heap.h>
#include <kernel/lib/string.h>
#include <kernel/lib/stdio.h>

/*
 * ============================================================================
 * PER-CPU GLOBAL REGISTRY
 * ============================================================================
 */
cpu_data_block_t *cpu_blocks[MAX_CPUS] = {0};

/*
 * ============================================================================
 * CRITICAL MSRs & DEFINITIONS
 * ============================================================================
 */
#define IA32_GS_BASE 0xC0000101
#define IA32_KERNEL_GS_BASE 0xC0000102

/*
 * ============================================================================
 * MSR (Model Specific Register) HELPER
 * ============================================================================
 */
static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);

    __asm__ volatile(
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
        : "memory");
}

/*
 * ============================================================================
 * GDT ENTRY CREATION HELPERS
 * ============================================================================
 */

// Cria uma entrada padrão de 8 bytes para Código e Dados (Ring 0 e Ring 3)
static uint64_t create_gdt_entry(uint32_t base, uint32_t limit, uint8_t access, uint8_t flags)
{
    uint64_t entry = 0;

    // Configuração do Limite (bits 0-15 e 48-51)
    entry |= (limit & 0xFFFF);
    entry |= ((uint64_t)(limit & 0xF0000) << 32);

    // Configuração da Base (bits 16-39 e 56-63)
    entry |= ((uint64_t)(base & 0xFFFFFF) << 16);
    entry |= ((uint64_t)(base & 0xFF000000) << 32);

    // Configuração do byte de Acesso (bits 40-47)
    entry |= ((uint64_t)access << 40);

    // Configuração das Flags (bits 52-55)
    entry |= ((uint64_t)(flags & 0x0F) << 52);

    return entry;
}

// Injeta o descritor especial de TSS de 16 bytes exigido pela arquitetura x86_64
static void write_gdt_tss_entry(uint64_t *gdt, uint32_t index, uint64_t tss_base, uint32_t tss_limit, uint8_t access)
{
    // Parte baixa: Estrutura semelhante a um segmento de dados comum
    uint64_t low = 0;
    low |= (tss_limit & 0xFFFF);
    low |= ((uint64_t)(tss_limit & 0xF0000) << 32);
    low |= ((uint64_t)(tss_base & 0xFFFFFF) << 16);
    low |= ((uint64_t)(tss_base & 0xFF000000) << 32);
    low |= ((uint64_t)access << 40);
    low |= ((uint64_t)0x00 << 52); // Granularidade limpa (L=0)

    // Parte alta: Contém os 32 bits superiores do endereço de 64 bits da estrutura TSS
    uint64_t high = (tss_base >> 32);

    gdt[index]     = low;
    gdt[index + 1] = high;
}

/*
 * ============================================================================
 * LOCAL CPU INITIALIZATION (ENTRY POINT)
 * ============================================================================
 */
void cpu_initialize_local(uint32_t cpu_id, uint32_t lapic_id, uint64_t stack_top)
{
    if (cpu_id >= MAX_CPUS) {
        kprintf("Erro: CPU ID %d excede o limite maximo.\n", cpu_id);
        return;
    }

    /*
     * ALINHAMENTO DO HEAP PARA HARDWARE REAL:
     * Alocamos espaço extra (+ 15) para forçar o ponteiro a alinhar em 16 bytes (~0xF).
     * O hardware real gera um Triple Fault imediato se tabelas de controlo de sistema 
     * estiverem em endereços desalinhados.
     */
    void *raw_ptr = kmalloc(sizeof(cpu_data_block_t) + 15);
    if (!raw_ptr) {
        kprintf("Erro: Falha catastrófica de memória no Core %d.\n", cpu_id);
        return;
    }

    // Alinha o ponteiro limpando os bits de offset inferiores
    cpu_data_block_t *cpu = (cpu_data_block_t *)(((uintptr_t)raw_ptr + 15) & ~0xF);
    memset(cpu, 0, sizeof(cpu_data_block_t));

    // Inicialização dos metadados Per-CPU
    cpu->cpu_id = cpu_id;
    cpu->lapic_id = lapic_id;
    cpu->kernel_stack_top = stack_top;
    cpu->self = cpu;

    // 1. Configuração dos segmentos base da GDT (Índices 0 a 4)
    cpu->gdt_entries[0] = create_gdt_entry(0, 0, 0, 0);             // Null Descriptor (0x00)
    cpu->gdt_entries[1] = create_gdt_entry(0, 0xFFFFF, 0x9A, 0x02); // Kernel Code 64 (0x08)
    cpu->gdt_entries[2] = create_gdt_entry(0, 0xFFFFF, 0x92, 0x00); // Kernel Data 64 (0x10)
    cpu->gdt_entries[3] = create_gdt_entry(0, 0xFFFFF, 0xFA, 0x02); // User Code 64   (0x18)
    cpu->gdt_entries[4] = create_gdt_entry(0, 0xFFFFF, 0xF2, 0x00); // User Data 64   (0x20)

    // 2. Configuração da TSS Local do Core (Pilha Ring 0 ativa em Interrupções vindo de Ring 3)
    cpu->tss.rsp0 = stack_top; 
    cpu->tss.iomap_base = sizeof(tss_t); // Desativa e bloqueia acessos diretos ao mapa I/O por defeito

    // 3. Instalação do descritor de TSS de 16 bytes na GDT (Ocupa os índices 5 e 6)
    // Seletor correspondente: 5 * 8 = 0x28
    write_gdt_tss_entry(cpu->gdt_entries, 5, (uint64_t)&cpu->tss, sizeof(tss_t) - 1, 0x89);

    // 4. Configuração do Descritor GDTR e Carga da GDT
    cpu->gdtr.limit = (sizeof(uint64_t) * GDT_ENTRIES) - 1;
    cpu->gdtr.base  = (uint64_t)&cpu->gdt_entries;

    __asm__ volatile("lgdt %0" : : "m"(cpu->gdtr) : "memory");

    // 5. Recarga dos registradores de segmento de dados (Limpeza de seletores antigos do bootloader)
    __asm__ volatile(
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%ss\n"
        "xor %%ax, %%ax\n" // Carrega o seletor NULO (0x00)
        "mov %%ax, %%gs\n" // O seletor nulo avisa o hardware x86-64 para confiar apenas no MSR GS_BASE
        : : : "rax", "memory"
    );

    // 6. Carga do Task Register (Carrega a TSS associada a este Core)
    // Passa o seletor 0x28 (Índice GDT 5, RPL 0)
    __asm__ volatile("ltr %%ax" : : "a"(0x28) : "memory");

    // 7. Configuração do MSR GS_BASE para habilitar os dados estruturados Per-CPU
    wrmsr(IA32_GS_BASE, (uint64_t)cpu);

    // 7. Configuração dos MSRs GS para Ring 0 e Ring 3
    wrmsr(IA32_GS_BASE, (uint64_t)cpu);
    wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)cpu);

    // Salva o ponteiro devidamente alinhado no array global
    cpu_blocks[cpu_id] = cpu;

    kprintf("CPU %d (LAPIC %d): GDT, TSS (0x28) e GS_BASE mapeados e operantes em Hardware. %p\n", 
            cpu_id, lapic_id, cpu);
}
