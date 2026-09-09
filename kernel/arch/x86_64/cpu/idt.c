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

// STUBS DAS IRQS DO IOAPIC HARDWARE (Exportados do seu interrupt.asm)
extern void isr33();  extern void isr34();  extern void isr35();  extern void isr36();
extern void isr37();  extern void isr38();  extern void isr39();  extern void isr40();
extern void isr41();  extern void isr42();  extern void isr43();  extern void isr44();
extern void isr45();  extern void isr46();  extern void isr47();  extern void isr48();
extern void isr49();  extern void isr50();  extern void isr51();  extern void isr52();
extern void isr53();  extern void isr54();  extern void isr55();  extern void isr56();
extern void isr57();  extern void isr58();  extern void isr59();  extern void isr60();
extern void isr61();  extern void isr62();  extern void isr63();  extern void isr64();
extern void isr65();  extern void isr66();  extern void isr67();  extern void isr68();
extern void isr69();  extern void isr70();  extern void isr71();  extern void isr72();
extern void isr73();  extern void isr74();  extern void isr75();  extern void isr76();
extern void isr77();  extern void isr78();  extern void isr79();  extern void isr80();

// DECLARAÇÃO DOS 32 STUBS EM ASSEMBLY PARA MENSAGENS MSI (VETORES 81 A 112)
extern void isr81();  extern void isr82();  extern void isr83();  extern void isr84();
extern void isr85();  extern void isr86();  extern void isr87();  extern void isr88();
extern void isr89();  extern void isr90();  extern void isr91();  extern void isr92();
extern void isr93();  extern void isr94();  extern void isr95();  extern void isr96();
extern void isr97();  extern void isr98();  extern void isr99();  extern void isr100();
extern void isr101(); extern void isr102(); extern void isr103(); extern void isr104();
extern void isr105(); extern void isr106(); extern void isr107(); extern void isr108();
extern void isr109(); extern void isr110(); extern void isr111(); extern void isr112();


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

    /* Mapeamento fixo do Relógio Core */
    idt_set_gate(32, (unsigned long)isr32, 0x08, 0x0E, 0, 0);   // LAPIC Timer

    /* 
     * MAPEAMENTO DAS IRQS DE HARDWARE EXTERNAS
     * Acopla e ativa os portões da IDT de 33 a 47 ligando os stubs ao despachante.
     * Mapeia de forma estrita as linhas físicas roteadas pelo IOAPIC.
     */
    idt_set_gate(33, (unsigned long)isr33, 0x08, 0x0E, 0, 0);
    idt_set_gate(34, (unsigned long)isr34, 0x08, 0x0E, 0, 0);
    idt_set_gate(35, (unsigned long)isr35, 0x08, 0x0E, 0, 0);
    idt_set_gate(36, (unsigned long)isr36, 0x08, 0x0E, 0, 0);
    idt_set_gate(37, (unsigned long)isr37, 0x08, 0x0E, 0, 0);
    idt_set_gate(38, (unsigned long)isr38, 0x08, 0x0E, 0, 0);
    idt_set_gate(39, (unsigned long)isr39, 0x08, 0x0E, 0, 0);
    idt_set_gate(40, (unsigned long)isr40, 0x08, 0x0E, 0, 0);
    idt_set_gate(41, (unsigned long)isr41, 0x08, 0x0E, 0, 0);
    idt_set_gate(42, (unsigned long)isr42, 0x08, 0x0E, 0, 0);
    idt_set_gate(43, (unsigned long)isr43, 0x08, 0x0E, 0, 0);
    idt_set_gate(44, (unsigned long)isr44, 0x08, 0x0E, 0, 0);
    idt_set_gate(45, (unsigned long)isr45, 0x08, 0x0E, 0, 0);
    idt_set_gate(46, (unsigned long)isr46, 0x08, 0x0E, 0, 0);
    idt_set_gate(47, (unsigned long)isr47, 0x08, 0x0E, 0, 0);
    idt_set_gate(48, (unsigned long)isr48, 0x08, 0x0E, 0, 0);
    idt_set_gate(49, (unsigned long)isr49, 0x08, 0x0E, 0, 0);
    idt_set_gate(50, (unsigned long)isr50, 0x08, 0x0E, 0, 0);
    idt_set_gate(51, (unsigned long)isr51, 0x08, 0x0E, 0, 0);
    idt_set_gate(52, (unsigned long)isr52, 0x08, 0x0E, 0, 0);
    idt_set_gate(53, (unsigned long)isr53, 0x08, 0x0E, 0, 0);
    idt_set_gate(54, (unsigned long)isr54, 0x08, 0x0E, 0, 0);
    idt_set_gate(55, (unsigned long)isr55, 0x08, 0x0E, 0, 0);
    idt_set_gate(56, (unsigned long)isr56, 0x08, 0x0E, 0, 0);
    idt_set_gate(57, (unsigned long)isr57, 0x08, 0x0E, 0, 0);
    idt_set_gate(58, (unsigned long)isr58, 0x08, 0x0E, 0, 0);
    idt_set_gate(59, (unsigned long)isr59, 0x08, 0x0E, 0, 0);
    idt_set_gate(60, (unsigned long)isr60, 0x08, 0x0E, 0, 0);
    idt_set_gate(61, (unsigned long)isr61, 0x08, 0x0E, 0, 0);
    idt_set_gate(62, (unsigned long)isr62, 0x08, 0x0E, 0, 0);
    idt_set_gate(63, (unsigned long)isr63, 0x08, 0x0E, 0, 0);
    idt_set_gate(64, (unsigned long)isr64, 0x08, 0x0E, 0, 0);
    idt_set_gate(65, (unsigned long)isr65, 0x08, 0x0E, 0, 0);
    idt_set_gate(66, (unsigned long)isr66, 0x08, 0x0E, 0, 0);
    idt_set_gate(67, (unsigned long)isr67, 0x08, 0x0E, 0, 0);
    idt_set_gate(68, (unsigned long)isr68, 0x08, 0x0E, 0, 0);
    idt_set_gate(69, (unsigned long)isr69, 0x08, 0x0E, 0, 0);
    idt_set_gate(70, (unsigned long)isr70, 0x08, 0x0E, 0, 0);
    idt_set_gate(71, (unsigned long)isr71, 0x08, 0x0E, 0, 0);
    idt_set_gate(72, (unsigned long)isr72, 0x08, 0x0E, 0, 0);
    idt_set_gate(73, (unsigned long)isr73, 0x08, 0x0E, 0, 0);
    idt_set_gate(74, (unsigned long)isr74, 0x08, 0x0E, 0, 0);
    idt_set_gate(75, (unsigned long)isr75, 0x08, 0x0E, 0, 0);
    idt_set_gate(76, (unsigned long)isr76, 0x08, 0x0E, 0, 0);
    idt_set_gate(77, (unsigned long)isr77, 0x08, 0x0E, 0, 0);
    idt_set_gate(78, (unsigned long)isr78, 0x08, 0x0E, 0, 0);
    idt_set_gate(79, (unsigned long)isr79, 0x08, 0x0E, 0, 0);
    idt_set_gate(80, (unsigned long)isr80, 0x08, 0x0E, 0, 0);

    // Mapeamento dos vetores msi (81 A 112)
    idt_set_gate(81,  (unsigned long)isr81,  0x08, 0x0E, 0, 0);
    idt_set_gate(82,  (unsigned long)isr82,  0x08, 0x0E, 0, 0);
    idt_set_gate(83,  (unsigned long)isr83,  0x08, 0x0E, 0, 0);
    idt_set_gate(84,  (unsigned long)isr84,  0x08, 0x0E, 0, 0);
    idt_set_gate(85,  (unsigned long)isr85,  0x08, 0x0E, 0, 0);
    idt_set_gate(86,  (unsigned long)isr86,  0x08, 0x0E, 0, 0);
    idt_set_gate(87,  (unsigned long)isr87,  0x08, 0x0E, 0, 0);
    idt_set_gate(88,  (unsigned long)isr88,  0x08, 0x0E, 0, 0);
    idt_set_gate(89,  (unsigned long)isr89,  0x08, 0x0E, 0, 0);
    idt_set_gate(90,  (unsigned long)isr90,  0x08, 0x0E, 0, 0);
    idt_set_gate(91,  (unsigned long)isr91,  0x08, 0x0E, 0, 0);
    idt_set_gate(92,  (unsigned long)isr92,  0x08, 0x0E, 0, 0);
    idt_set_gate(93,  (unsigned long)isr93,  0x08, 0x0E, 0, 0);
    idt_set_gate(94,  (unsigned long)isr94,  0x08, 0x0E, 0, 0);
    idt_set_gate(95,  (unsigned long)isr95,  0x08, 0x0E, 0, 0);
    idt_set_gate(96,  (unsigned long)isr96,  0x08, 0x0E, 0, 0);
    idt_set_gate(97,  (unsigned long)isr97,  0x08, 0x0E, 0, 0);
    idt_set_gate(98,  (unsigned long)isr98,  0x08, 0x0E, 0, 0);
    idt_set_gate(99,  (unsigned long)isr99,  0x08, 0x0E, 0, 0);
    idt_set_gate(100, (unsigned long)isr100, 0x08, 0x0E, 0, 0);
    idt_set_gate(101, (unsigned long)isr101, 0x08, 0x0E, 0, 0);
    idt_set_gate(102, (unsigned long)isr102, 0x08, 0x0E, 0, 0);
    idt_set_gate(103, (unsigned long)isr103, 0x08, 0x0E, 0, 0);
    idt_set_gate(104, (unsigned long)isr104, 0x08, 0x0E, 0, 0);
    idt_set_gate(105, (unsigned long)isr105, 0x08, 0x0E, 0, 0);
    idt_set_gate(106, (unsigned long)isr106, 0x08, 0x0E, 0, 0);
    idt_set_gate(107, (unsigned long)isr107, 0x08, 0x0E, 0, 0);
    idt_set_gate(108, (unsigned long)isr108, 0x08, 0x0E, 0, 0);
    idt_set_gate(109, (unsigned long)isr109, 0x08, 0x0E, 0, 0);
    idt_set_gate(110, (unsigned long)isr110, 0x08, 0x0E, 0, 0);
    idt_set_gate(111, (unsigned long)isr111, 0x08, 0x0E, 0, 0);
    idt_set_gate(112, (unsigned long)isr112, 0x08, 0x0E, 0, 0);


    /* Mapeamento de canais de exceções internas do APIC */
    idt_set_gate(254, (unsigned long)isr254, 0x08, 0x0E, 0, 0); // LVT Error
    idt_set_gate(255, (unsigned long)isr255, 0x08, 0x0E, 0, 0); // Spurious Vector


    // Configuração do IDTR e carga no processador
    g_idtr.limit = (sizeof(idt_t) * IDT_MAX_ENTRIES) - 1;
    g_idtr.base  = (unsigned long)&g_idt;

    __asm__ __volatile__("lidt %0" : : "m"(g_idtr));

    kprintf("[IDT] Excecoes, Timer (32), Erro (254) e Spurious (255) carregados.\n");
}
