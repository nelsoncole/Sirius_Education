/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: idt.c
 *    Description: Inicialização da Interrupt Descriptor Table (IDT),
 *                 preenchimento dinâmico de gates via bitfields e carga
 *                 do registador IDTR em ambiente x86_64.
 * 
 *         Author: Nelson Cole
 *   Created Date: 31/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 31/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/cpu/idt.h>
#include <kernel/lib/string.h>
#include <kernel/lib/stdio.h>

// Declaração dos stubs do Assembly para o compilador C saber onde eles começam
// Exceptions Faults
extern void isr0();  extern void isr1();  extern void isr2();  extern void isr3();
extern void isr4();  extern void isr5();  extern void isr6();  extern void isr7();
extern void isr8();  extern void isr9();  extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();


// LAPIC
extern void isr32(void);  // O Stub Assembly do LAPIC Timer
extern void isr254(void); // Handler de Erro do LAPIC
extern void isr255(void); // Handler de Spurious do LAPIC


// A tabela IDT global com as 256 entradas exigidas pela arquitetura x86
idt_t g_idt[IDT_MAX_ENTRIES] __attribute__((aligned(16)));
idtr_t g_idtr;

// Declaração externa da variável global para contar os tiques do sistema
extern unsigned long g_system_ticks;

/*
 * Configura uma entrada (Gate) específica na IDT utilizando os campos de bits.
 * 
 * Parâmetros:
 *   vector:    O índice da interrupção (0 a 255)
 *   handler:   O endereço virtual da função que vai tratar a interrupção (ISR)
 *   selector:  O seletor de segmento de código na GDT (Geralmente 0x08 para Ring 0)
 *   type:      O tipo de Gate (0x0E = 64-bit Interrupt Gate, 0x0F = 64-bit Trap Gate)
 *   dpl:       Nível de privilégio exigido (0 = Kernel, 3 = User)
 *   ist_index: Índice da pilha IST no TSS (0 = Não usa, 1 = IST1 para falhas críticas)
 */
void idt_set_gate(uint8_t vector, unsigned long handler, uint16_t selector, uint8_t type, uint8_t dpl, uint8_t ist_index)
{
    // Injeta o seletor de código da GDT e ativa o bit Presente (p = 1)
    g_idt[vector].sel    = selector;
    g_idt[vector].p      = 1;
    g_idt[vector].dpl    = dpl & 3;   // Garante isolamento de 2 bits
    g_idt[vector].type   = type & 31; // Garante isolamento de 5 bits (0x0E / 11110b)
    g_idt[vector].ist    = ist_index & 7; // Mapeia o índice do array 'ist' do TSS
    g_idt[vector].unused = 0;
    g_idt[vector].reserved = 0;

    // Fatiamento cirúrgico do endereço do Handler de 64 bits nos bitfields da IDT
    g_idt[vector].offset_15_0  = (handler & 0xFFFFUL);
    g_idt[vector].offset_31_16 = ((handler >> 16) & 0xFFFFUL);
    g_idt[vector].offset_63_32 = ((handler >> 32) & 0xFFFFFFFFUL);
}

/*
 * Inicializa a IDT, limpa a memória contra lixo, configura os limites no
 * registador IDTR e carrega a tabela no processador atual.
 */
void idt_init(void)
{
    // Limpar variável global para contar os tiques do sistema
    g_system_ticks = 0;

    // Limpa a tabela
    memset(&g_idt, 0, sizeof(g_idt));

    // Mapeamento manual e estrito das 32 exceções nativas da CPU
    // Atributos: Seletor 0x08 (Kernel Code), Tipo 0x0E (Interrupt Gate), DPL 0 (Kernel)
    idt_set_gate(0,  (unsigned long)isr0,  0x08, 0x0E, 0, 0);
    idt_set_gate(1,  (unsigned long)isr1,  0x08, 0x0E, 0, 0);
    idt_set_gate(2,  (unsigned long)isr2,  0x08, 0x0E, 0, 0);
    idt_set_gate(3,  (unsigned long)isr3,  0x08, 0x0E, 0, 0);
    idt_set_gate(4,  (unsigned long)isr4,  0x08, 0x0E, 0, 0);
    idt_set_gate(5,  (unsigned long)isr5,  0x08, 0x0E, 0, 0);
    idt_set_gate(6,  (unsigned long)isr6,  0x08, 0x0E, 0, 0);
    idt_set_gate(7,  (unsigned long)isr7,  0x08, 0x0E, 0, 0);
    
    // EXCEÇÃO CRÍTICA: Double Fault (#DF) ganha a IST 1 (Pilha isolada alocada no seu Heap)
    idt_set_gate(8,  (unsigned long)isr8,  0x08, 0x0E, 0, 1); 
    
    idt_set_gate(9,  (unsigned long)isr9,  0x08, 0x0E, 0, 0);
    idt_set_gate(10, (unsigned long)isr10, 0x08, 0x0E, 0, 0);
    idt_set_gate(11, (unsigned long)isr11, 0x08, 0x0E, 0, 0);
    idt_set_gate(12, (unsigned long)isr12, 0x08, 0x0E, 0, 0);
    idt_set_gate(13, (unsigned long)isr13, 0x08, 0x0E, 0, 0); // General Protection Fault
    idt_set_gate(14, (unsigned long)isr14, 0x08, 0x0E, 0, 0); // Page Fault
    idt_set_gate(15, (unsigned long)isr15, 0x08, 0x0E, 0, 0);
    idt_set_gate(16, (unsigned long)isr16, 0x08, 0x0E, 0, 0);
    idt_set_gate(17, (unsigned long)isr17, 0x08, 0x0E, 0, 0);
    idt_set_gate(18, (unsigned long)isr18, 0x08, 0x0E, 0, 0);
    idt_set_gate(19, (unsigned long)isr19, 0x08, 0x0E, 0, 0);
    idt_set_gate(20, (unsigned long)isr20, 0x08, 0x0E, 0, 0);
    idt_set_gate(21, (unsigned long)isr21, 0x08, 0x0E, 0, 0);
    idt_set_gate(22, (unsigned long)isr22, 0x08, 0x0E, 0, 0);
    idt_set_gate(23, (unsigned long)isr23, 0x08, 0x0E, 0, 0);
    idt_set_gate(24, (unsigned long)isr24, 0x08, 0x0E, 0, 0);
    idt_set_gate(25, (unsigned long)isr25, 0x08, 0x0E, 0, 0);
    idt_set_gate(26, (unsigned long)isr26, 0x08, 0x0E, 0, 0);
    idt_set_gate(27, (unsigned long)isr27, 0x08, 0x0E, 0, 0);
    idt_set_gate(28, (unsigned long)isr28, 0x08, 0x0E, 0, 0);
    idt_set_gate(29, (unsigned long)isr29, 0x08, 0x0E, 0, 0);
    idt_set_gate(30, (unsigned long)isr30, 0x08, 0x0E, 0, 0);
    idt_set_gate(31, (unsigned long)isr31, 0x08, 0x0E, 0, 0);

    idt_set_gate(32, (unsigned long)isr32, 0x08, 0x0E, 0, 0);   // LAPIC Timer
    idt_set_gate(254, (unsigned long)isr254, 0x08, 0x0E, 0, 0); // LVT Error
    idt_set_gate(255, (unsigned long)isr255, 0x08, 0x0E, 0, 0); // Spurious Vector


    // Configuração do IDTR e carga no processador
    g_idtr.limit = (sizeof(idt_t) * IDT_MAX_ENTRIES) - 1;
    g_idtr.base  = (unsigned long)&g_idt;

    __asm__ __volatile__("lidt %0" : : "m"(g_idtr));

    kprintf("[IDT] Excecoes, Timer (32), Erro (254) e Spurious (255) carregados.\n");
}
