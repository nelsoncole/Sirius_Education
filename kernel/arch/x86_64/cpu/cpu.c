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

// Variável global: 0 = Não presente/Inativo, 1 = Presente e Ativo no Kernel
volatile int g_cpu_has_avx2 = 0;

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

static void arch_init_fpu_core(void) {
    //  Configurar o CR4
    uint64_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 9);   // OSFXSR: Ativa fxsave/fxrstor e os 16 registos XMM
    cr4 |= (1 << 10);  // OSXMMEXCPT: Ativa suporte a exceções SIMD unmasked (#XM)
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));

    // Inicializar o FPU clássico (x87)
    __asm__ volatile("fninit");
}

/**
 * @brief Ativa o bit TS (Task Switched) no CR0.
 */
void arch_fpu_set_ts(void) {
    uint64_t cr0;
    __asm__ __volatile__(
        "movq %%cr0, %0\n\t"
        "orq $8, %0\n\t"       // 8 = bit 3 (TS)
        "movq %0, %%cr0"
        : "=r"(cr0)
        :
        : "memory"             // Impede o GCC de reordenar esta escrita
    );
}

/**
 * @brief Limpa o bit TS (Task Switched) no CR0.
 *        Usa uma barreira de memória total para garantir a execução imediata.
 */
void arch_fpu_clear_ts(void) {
    __asm__ __volatile__(
        "clts" 
        : 
        : 
        : "memory"             // Força o pipeline da CPU a esvaziar antes do próximo comando
    );
}


int check_avx_hardware_support(void) {
    uint32_t eax, ebx, ecx, edx;

    // Verificar suporte a XSAVE e AVX básicos
    // Chamar CPUID com EAX = 1
    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));

    // Bit 26 de ECX: Suporte a XSAVE (obrigatório para gerir o estado do AVX)
    // Bit 28 de ECX: Suporte a AVX básico
    if (!(ecx & (1 << 26)) || !(ecx & (1 << 28))) {
        return 0; // Hardware não suporta AVX ou XSAVE
    }

    // Verificar suporte específico a AVX2
    // Chamar CPUID com EAX = 7, ECX = 0
    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(7), "c"(0));

    // Bit 5 de EBX: Suporte a AVX2
    if (!(ebx & (1 << 5))) {
        return 0; // Suporta AVX de 256-bits, mas NÃO suporta AVX2 (inteiros/vmovntdqa)
    }

    return 1; // Hardware 100% compatível com AVX2 e XSAVE
}

void enable_avx_features(void) {
    // 1. Ativar o bit OSXSAVE (bit 18) no registo CR4
    __asm__ __volatile__(
        "mov %%cr4, %%rax\n"
        "or $0x40000, %%rax\n" // 1 << 18
        "mov %%rax, %%cr4\n"
        : : : "rax"
    );

    // 2. Configurar o registo XCR0 (Extended Control Register 0)
    uint32_t ecx = 0;
    uint32_t eax, edx;
    
    // Ler o estado atual do XCR0
    __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(ecx));
    
    // Ativar x87(bit0), SSE/XMM(bit1) e AVX/YMM(bit2)
    eax |= (1 << 0) | (1 << 1) | (1 << 2); 
    
    // Escrever as permissões de volta na CPU
    __asm__ __volatile__("xsetbv" : : "a"(eax), "d"(edx), "c"(ecx));
}

/*
 * ============================================================================
 * LOCAL CPU INITIALIZATION (ENTRY POINT)
 * ============================================================================
 */
void cpu_initialize_local(uint32_t cpu_id, uint32_t lapic_id, uint64_t stack_top)
{
    arch_init_fpu_core();

    if(check_avx_hardware_support()) 
    {
        enable_avx_features();

        if(!g_cpu_has_avx2)g_cpu_has_avx2  = 1;
        kprintf("[CPU] Sucesso: AVX2 detetado e ativado. Registos YMM prontos.\n");
    }
    
    kprintf("[SMP] Nucleo %u (LAPIC ID: %u) Stack %lX!\n", cpu_id, lapic_id, stack_top);
    
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
    // Captura o registo CR3 físico atual do Kernel puro no boot
    uint64_t current_cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(current_cr3));
    // Grava no bloco local para uso assíncrono do Escalonador
    cpu->cr3 = current_cr3;
    cpu->self = cpu;

    // Configuração dos segmentos base da GDT (Índices 0 a 3)
    cpu->gdt_entries[0] = create_gdt_entry(0, 0, 0, 0);             // Null (0x00)
    cpu->gdt_entries[1] = create_gdt_entry(0, 0xFFFFF, 0x9A, 0x02); // Kernel Code 64 (0x08)
    cpu->gdt_entries[2] = create_gdt_entry(0, 0xFFFFF, 0x92, 0x00); // Kernel Data 64 (0x10)

    // ============================================================================
    // SEQUÊNCIA EXIGIDA PELO SYSRET (Índices 3, 4 e 5)
    // ============================================================================
    cpu->gdt_entries[3] = create_gdt_entry(0, 0xFFFFF, 0xFA, 0x02); // BASE VAZIA 32-bit (0x18 -> 0x1B com RPL 3)
    cpu->gdt_entries[4] = create_gdt_entry(0, 0xFFFFF, 0xF2, 0x00); // USER SS REAL 64-bit (0x20 -> 0x23 com RPL 3)
    cpu->gdt_entries[5] = create_gdt_entry(0, 0xFFFFF, 0xFA, 0x02); // USER CS REAL 64-bit (0x28 -> 0x2B com RPL 3)

    // Configuração da TSS Local do Core (Pilha Ring 0 ativa em Interrupções vindo de Ring 3)
    cpu->tss.rsp0 = stack_top;

    cpu->tss.iomap_base = sizeof(tss_t); // Desativa e bloqueia acessos diretos ao mapa I/O por defeito

    // Instalação da TSS (Passa para os Índices 6 e 7)
    // Seletor correspondente: 6 * 8 = 0x30. Totalmente isolado e seguro!
    write_gdt_tss_entry(cpu->gdt_entries, 6, (uint64_t)&cpu->tss, sizeof(tss_t) - 1, 0x89);


    // Configuração do Descritor GDTR e Carga da GDT
    cpu->gdtr.limit = (sizeof(uint64_t) * GDT_ENTRIES) - 1;
    cpu->gdtr.base  = (uint64_t)&cpu->gdt_entries;

    __asm__ volatile("lgdt %0" : : "m"(cpu->gdtr) : "memory");

    // Recarga dos registradores de segmento de dados (Limpeza de seletores antigos do bootloader)
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

    // Carga do Task Register (Carrega a TSS associada a este Core)
    // Passa o seletor 0x30 (Índice GDT 6, RPL 0)
    __asm__ volatile("ltr %%ax" : : "a"(0x30) : "memory");

    // Configuração do MSR GS_BASE para habilitar os dados estruturados Per-CPU
    wrmsr(IA32_GS_BASE, (uint64_t)cpu);
    wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)cpu);

    // Salva o ponteiro devidamente alinhado no array global
    cpu_blocks[cpu_id] = cpu;

    kprintf("CPU %d (LAPIC %d): GDT, TSS (0x30) com IST1 ativo e operantes em Hardware. %p\n", 
            cpu_id, lapic_id, cpu);
}

/**
 * Retorna o bloco de dados de um CPU específico através do seu ID.
 */
cpu_data_block_t* get_cpu_data_block(uint32_t cpu_id)
{
    if (cpu_id >= MAX_CPUS)
    {
        return NULL; /* ID inválido ou acima do limite máximo */
    }
    
    return cpu_blocks[cpu_id];
}