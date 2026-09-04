/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: isr.c
 *    Description: Handler central em C para processamento de exceções
 *                 e interrupções da arquitetura x86_64 com diagnóstico Per-CPU.
 *
 *         Author: Nelson Cole
 *   Created Date: 31/08/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 04/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/cpu/idt.h>
#include <kernel/arch/x86_64/cpu/lapic.h>
#include <kernel/arch/x86_64/cpu/reg.h>
#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/lib/stdio.h>

// Variável global para contar os tiques do sistema
unsigned long g_system_ticks = 0;

// Lista com as strings de diagnóstico das 32 exceções nativas da CPU Intel/AMD
const char *exception_messages[] = {
    "Division By Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
    "Virtualization Exception",
    "Control Protection",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"};

/*
 * O Handler Central em C chamado pelo stub em Assembly.
 */
void interrupt_handler_c(registers_t *regs)
{
    // Processa apenas as exceções nativas de hardware (0 a 31)
    if (regs->int_no < 32)
    {
        /*
         * ====================================================================
         * LEITURA DINÂMICA DO CPU_ID VIA GS (PER-CPU)
         *
         * Lemos os primeiros 4 bytes (uint32_t) apontados pela base GS.
         * Como o cpu_id está no início da cpu_data_block_t, o offset é 0.
         * ====================================================================
         */
        unsigned long current_cpu_id = get_current_cpu_id();

        kprintf("\n========================================================================\n");
        kprintf(" !!! EXCECAO CRITICA DO PROCESSADOR DETECTADA [ CORE %lu ] !!!\n", current_cpu_id);
        kprintf("========================================================================\n");
        kprintf(" Excecao N.: %lu -> [ #%s ]\n", regs->int_no, exception_messages[regs->int_no]);
        kprintf(" Cod. Erro : 0x%lX\n", regs->error_code);
        kprintf("------------------------------------------------------------------------\n");
        kprintf(" REGISTRADORES DE EXECUCAO (Long Mode):\n");
        kprintf("  * RIP: 0x%lX  |  CS : 0x%lX  |  RFLAGS: 0x%lX\n", regs->rip, regs->cs, regs->rflags);
        kprintf("  * RSP: 0x%lX  |  SS : 0x%lX\n", regs->rsp, regs->ss);
        kprintf("  * RAX: 0x%lX  |  RBX: 0x%lX  |  RCX   : 0x%lX  |  RDX: 0x%lX\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
        kprintf("  * RDI: 0x%lX  |  RSI: 0x%lX  |  RBP   : 0x%lX\n", regs->rdi, regs->rsi, regs->rbp);
        kprintf("  * R8 : 0x%lX  |  R9 : 0x%lX  |  R10   : 0x%lX  |  R11: 0x%lX\n", regs->r8, regs->r9, regs->r10, regs->r11);
        kprintf("  * R12: 0x%lX  |  R13: 0x%lX  |  R14   : 0x%lX  |  R15: 0x%lX\n", regs->r12, regs->r13, regs->r14, regs->r15);

        // Se for um Page Fault (#PF, Vetor 14), capturamos o endereço linear falho no CR2
        if (regs->int_no == 14)
        {
            unsigned long cr2_val;
            __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2_val));
            kprintf("  * Endereco da Falha de Pagina (CR2): 0x%lX\n", cr2_val);
        }
        kprintf("========================================================================\n");
        kprintf(" Kernel em estado de panico controlado. Sistema suspenso.\n");

        // Desativa interrupções e congela a CPU atual em loop infinito
        for (;;)
        {
            __asm__ __volatile__("cli");
            __asm__ __volatile__("hlt");
        }
    }

    if (regs->int_no == 32)
    {
        // Leitura rápida e segura do ID do núcleo atual via GS
        uint32_t current_cpu_id = get_current_cpu_id();

        // 1. RESPONSABILIDADES GLOBAIS (Apenas o BSP / Core 0)
        if (current_cpu_id == 0)
        {
            g_system_ticks++;
            if (g_system_ticks % 100 == 0)
            {
                kprintf(".");
            }
        }

        // 2. MULTITASKING LOCAL DO NÚCLEO (Todos os cores entram aqui!)
        /*
         * Aqui o BSP (Core 0) e todos os APs (Cores 1+) executam de forma
         * independente as suas rotinas locais de troca de contexto.
         * Exemplo: task_switch(regs);
         */

        // 3. FINALIZAÇÃO DA INTERRUPÇÃO DE HARDWARE
        lapic_eoi(); // Cada chip LAPIC limpa o seu próprio sinal individual de interrupção
        return;
    }

    // TRATAMENTO DO ERRO DO LOCAL APIC (Vetor 254)
    if (regs->int_no == 254)
    {
        kprintf("[LAPIC] Alerta de erro interno detetado por hardware!\n");
        lapic_eoi(); // Avisa o chip que a interrupção foi processada
        return;
    }

    // TRATAMENTO DE INTERRUPÇÃO ESPÚRIA (Vetor 255)
    if (regs->int_no == 255)
    {
        // Interrupções espúrias devem ser descartadas em silêncio absoluto.
        // NOTA DE HARDWARE: Não se deve enviar o comando EOI para o registador
        // do LAPIC em interrupções espúrias (especificação oficial da Intel).
        return;
    }
}