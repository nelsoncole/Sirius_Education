/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: cpu.c
 *    Description: Implementação da gestão estruturada por núcleo (Per-CPU).
 *                 Cria e inicializa dinamicamente as tabelas GDT locais,
 *                 configura os descritores expansíveis de 16 bits para o TSS,
 *                 isola as pilhas de interrupção do Kernel e injeta o endereço 
 *                 de cada bloco CpuDataBlock no registador MSR IA32_GS_BASE,
 *                 viabilizando suporte SMP escalável para até 256 núcleos.
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

#include <kernel/arch/cpu.h>
#include <kernel/string.h>

cpu_data_block_t* cpu_blocks[MAX_CPUS] = {0};

// Função auxiliar para escrever nos MSRs do processador
static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)val;
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ __volatile__("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

// Helper para construir entradas normais da GDT (Código e Dados) de 8 bytes
static uint64_t create_gdt_entry(uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    uint64_t entry = 0;

    // Em x86_64 a maior parte destes campos é ignorada para código/dados,
    // mas os bits de acesso/flags são obrigatórios

    entry |= ((uint64_t)limit & 0xFFFF);
    entry |= (((uint64_t)base & 0xFFFF) << 16);
    entry |= (((uint64_t)base & 0xFF0000) << 16);
    entry |= (((uint64_t)access) << 40);
    entry |= (((uint64_t)(limit >> 16) & 0x0F) << 48);
    entry |= (((uint64_t)flags & 0x0F) << 52);
    entry |= (((uint64_t)base >> 24) << 56);

    return entry;
}

void cpu_initialize_local(uint32_t cpu_id, uint32_t lapic_id, uint64_t stack_top) {
    cpu_data_block_t* block = cpu_blocks[cpu_id];
    
    block->cpu_id = cpu_id;
    block->lapic_id = lapic_id;
    block->kernel_stack_top = stack_top;

    // Configuração da GDT local (Ring 0 e Ring 3)
    // Null Descriptor (Obrigatório)
    block->gdt_entries[0] = 0;
    
    // Kernel Code (0x08): Access=0x9A (Present, Ring 0, Exec/Read),
    // Flags=0x2 (Long Mode)
    block->gdt_entries[1] = create_gdt_entry(0, 0xFFFFF, 0x9A, 0x2);
    
    // Kernel Data (0x10): Access=0x92 (Present, Ring 0, Read/Write), Flags=0x0
    block->gdt_entries[2] = create_gdt_entry(0, 0xFFFFF, 0x92, 0x0);
    
    // User Code   (0x1B): Access=0xFA (Present, Ring 3, Exec/Read),
    // Flags=0x2 (Long Mode) - Seletor 0x18 | 3
    block->gdt_entries[3] = create_gdt_entry(0, 0xFFFFF, 0xFA, 0x2);

    // User Data   (0x23): Access=0xF2 (Present, Ring 3, Read/Write),
    // Flags=0x0       - Seletor 0x20 | 3
    block->gdt_entries[4] = create_gdt_entry(0, 0xFFFFF, 0xF2, 0x0);

    // Descritor do TSS (Ocupa 16 bytes: gdt_entries[5] e gdt_entries[6])
    uint64_t tss_base = (uint64_t)&block->tss;
    uint32_t tss_limit = sizeof(tss_t) - 1;

    // Parte Baixa do Descritor de TSS (8 bytes): Access=0x89 (TSS Disponível), Flags=0x0
    block->gdt_entries[5] = ((uint64_t)tss_limit & 0xFFFF) |
                            (((uint64_t)tss_base & 0xFFFF) << 16) |
                            ((((uint64_t)tss_base >> 16) & 0xFF) << 32) |
                            (0x89ULL << 40) |
                            ((((uint64_t)tss_limit >> 16) & 0x0F) << 48) |
                            (((uint64_t)tss_base >> 24) << 56);

    // Parte Alta do Descritor de TSS (8 bytes): Guarda os 32 bits superiores do endereço de base
    block->gdt_entries[6] = (tss_base >> 32);
    
    // Entrada 7 fica a zero (alinhamento obrigatório de 16 bytes para o TSS)
    block->gdt_entries[7] = 0;

    // Primeiro limpamos o TSS para garantir que não há lixo de memória
    memset(&block->tss, 0, sizeof(tss_t));

    // Apontar o TSS para a Stack correta
    // Stack carregada ao transitar do Ring 3 -> Ring 0
    block->tss.rsp0 = stack_top;

    // Configurar o limite e base no GDTR local
    block->gdtr.limit = (sizeof(uint64_t) * 8) - 1;
    block->gdtr.base  = (uint64_t)&block->gdt_entries;

    // Carregar a GDT e o TSS neste core específico
    __asm__ __volatile__("lgdt %0" : : "m"(block->gdtr));
        
    // Carrega o seletor do TSS (Índice 5 na GDT = offset 0x28)
    // Usamos o seletor 0x28 (5 * 8 = 40 = 0x28)
    __asm__ __volatile__("ltr %%ax" : : "a"(0x28));

    
    /*
     * ========================================================
     * O TRUQUE PARA O MULTIPROCESSADOR
     *
     * Grava o endereço de memória virtual DESTE bloco no MSR
     * IA32_GS_BASE (0xC0000101) do core atual. A partir de 
     * agora, este processador pode usar instruções relativas 
     * a GS para aceder instantaneamente aos seus dados locais.
     * ========================================================
     */

    wrmsr(0xC0000101, (uint64_t)block); 
}