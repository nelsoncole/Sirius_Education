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
#include <kernel/kernel/sched/scheduler.h>

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
void* interrupt_handler_c(registers_t *regs)
{
    // Processa apenas as exceções nativas de hardware (0 a 31)
    if (regs->int_no < 32)
    {
        /*
         * ====================================================================
         * LEITURA DINÂMICA DO CONTEXTO DE EXECUÇÃO VIA PER-CPU
         * ====================================================================
         */
        unsigned long current_cpu_id = get_current_cpu_id();
        cpu_data_block_t *cpu = get_current_cpu();

        uint32_t active_pid = 0;
        uint32_t active_tid = 0;

        /* Se houver uma tarefa ativa no núcleo, extrai a sua identidade */
        if (cpu && cpu->current_thread)
        {
            active_tid = cpu->current_thread->tid;

            /* Verifica se a thread pertence a um processo utilizador isolado */
            if (cpu->current_thread->owner)
            {
                active_pid = cpu->current_thread->owner->pid;
            }
        }

        kprintf("\n========================================================================\n");
        kprintf(" !!! EXCECAO CRITICA DO PROCESSADOR DETECTADA [ CORE %lu ] !!!\n", current_cpu_id);
        kprintf("========================================================================\n");
        kprintf(" Excecao N.: %lu -> [ #%s ]\n", regs->int_no, exception_messages[regs->int_no]);
        kprintf(" Cod. Erro : 0x%lX\n", regs->error_code);

        if (active_pid > 0)
        {
            kprintf(" Origem    : PID: %u | TID: %u (Espaço de Utilizador / Ring 3)\n", active_pid, active_tid);
        }
        else if (active_tid > 0)
        {
            kprintf(" Origem    : TID: %u (Linha de Execução do Kernel / Ring 0)\n", active_tid);
        }
        else
        {
            kprintf(" Origem    : Inicialização Primitiva / Idle Thread Transitória\n");
        }

        kprintf("------------------------------------------------------------------------\n");
        kprintf(" REGISTRADORES DE EXECUCAO (Long Mode):\n");
        kprintf("  * RIP: 0x%016lX  |  CS : 0x%lX  |  RFLAGS: 0x%lX\n", regs->rip, regs->cs, regs->rflags);
        kprintf("  * RSP: 0x%016lX  |  SS : 0x%lX\n", regs->rsp, regs->ss);
        kprintf("  * RAX: 0x%lX  |  RBX: 0x%lX  |  RCX   : 0x%lX  |  RDX: 0x%lX\n", regs->rax, regs->rbx, regs->rcx, regs->rdx);
        kprintf("  * RDI: 0x%lX  |  RSI: 0x%lX  |  RBP   : 0x%lX\n", regs->rdi, regs->rsi, regs->rbp);
        kprintf("  * R8 : 0x%lX  |  R9 : 0x%lX  |  R10   : 0x%lX  |  R11: 0x%lX\n", regs->r8, regs->r9, regs->r10, regs->r11);
        kprintf("  * R12: 0x%lX  |  R13: 0x%lX  |  R14   : 0x%lX  |  R15: 0x%lX\n", regs->r12, regs->r13, regs->r14, regs->r15);
        kprintf("------------------------------------------------------------------------\n");
        struct gdtr
        {
            uint16_t limit;
            uint64_t base;
        } __attribute__((packed));

        struct gdtr gdtr;

        __asm__ __volatile__("sgdt %0" : "=m"(gdtr));

        kprintf("GDTR.base  = 0x%016lX\n", gdtr.base);
        kprintf("GDTR.limit = 0x%04X\n", gdtr.limit);

        uint64_t *gdt = (uint64_t *)gdtr.base;

        kprintf("GDT[5] = 0x%016lX\n", gdt[5]);
        kprintf("GDT[6] = 0x%016lX\n", gdt[6]);

        uint32_t lo, hi;

        __asm__ __volatile__(
            "mov $0xC0000081, %%ecx\n"
            "rdmsr\n"
            : "=a"(lo), "=d"(hi)
            :
            : "ecx");

        uint64_t star = ((uint64_t)hi << 32) | lo;

        kprintf("IA32_STAR = 0x%016lX\n", star);
        // Se for um Page Fault (#PF, Vetor 14), capturamos o endereço linear falho no CR2
        if (regs->int_no == 14)
        {
            unsigned long cr2_val;
            __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2_val));

            kprintf("\n==================================================\n");
            kprintf("                 CRASH: PAGE FAULT                \n");
            kprintf("==================================================\n");
            kprintf("Endereço Virtual Falho (CR2): 0x%016lX\n", cr2_val);
            kprintf("Código de Erro Bruto (ERR):   0x%lX\n", regs->error_code);
            kprintf("--------------------------------------------------\n");

            /* Decodificação física dos bits do Código de Erro x86_64 */
            kprintf("Causa:      %s\n", (regs->error_code & (1ULL << 0)) ? "Violação de Proteção" : "Página Não Presente");
            kprintf("Privilégio: %s\n", (regs->error_code & (1ULL << 2)) ? "Ring 3 (User Space)" : "Ring 0 (Kernel Space)");
            kprintf("Operação:   %s\n", (regs->error_code & (1ULL << 4)) ? "Busca de Instrução (Execute)" : "Leitura / Escrita");
            kprintf("==================================================\n");
        }
        kprintf("========================================================================\n");
        kprintf("Kernel em estado de panico controlado. Sistema suspenso.\n");

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
                //kprintf(".");
            }
        }

        // 3. FINALIZAÇÃO DA INTERRUPÇÃO DE HARDWARE (Antecipada para SMP)
        // Avisamos o LAPIC local que a interrupção foi processada ANTES de trocar o stack.
        // Assim, quando a nova thread herdar o CPU e reativar as interrupções, o LAPIC estará livre.
        lapic_eoi();

        // 2. MULTITASKING LOCAL DO NÚCLEO (Todos os cores entram aqui!)
        /*
         * O BSP (Core 0) e todos os APs (Cores 1+) executam de forma
         * independente as suas rotinas locais de troca de contexto.
         */

        void* next_stack = task_switch(regs);

        // Retorna a nova pilha para o interrupt.asm fazer o "mov rsp, rax"
        return next_stack;
    }

    // TRATAMENTO DO ERRO DO LOCAL APIC (Vetor 254)
    if (regs->int_no == 254)
    {
        kprintf("[LAPIC] Alerta de erro interno detetado por hardware!\n");
        lapic_eoi(); // Avisa o chip que a interrupção foi processada
        return regs;
    }

    // TRATAMENTO DE INTERRUPÇÃO ESPÚRIA (Vetor 255)
    if (regs->int_no == 255)
    {
        // Interrupções espúrias devem ser descartadas em silêncio absoluto.
        // NOTA DE HARDWARE: Não se deve enviar o comando EOI para o registador
        // do LAPIC em interrupções espúrias (especificação oficial da Intel).
        return regs;
    }

    // Retorna a pilha intata para os vetores que não alteram o contexto
    return regs;
}