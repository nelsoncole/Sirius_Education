/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ioapic.c
 *    Description: Inicialização e roteamento de interrupções de hardware externos
 *                 através do I/O Advanced Programmable Interrupt Controller (IOAPIC).
 * 
 *         Author: Nelson Cole
 *   Created Date: 31/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 10/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/cpu/ioapic.h>
#include <kernel/kernel/core/panic.h>
#include <kernel/lib/stdio.h>

// Ponteiro virtual global para os registadores MMIO de base do IOAPIC
static volatile uint32_t *g_ioapic = 0;

void* vmm_map_device(unsigned long phys_addr, unsigned long size);

/*
 * Escreve num registador interno do IOAPIC usando o esquema Select/Window
 */
static void ioapic_write(uint8_t reg, uint32_t value) {
    g_ioapic[IOAPIC_REG_SEL / 4] = reg;
    g_ioapic[IOAPIC_REG_WIN / 4] = value;
}

/*
 * Lê um registador interno do IOAPIC usando o esquema Select/Window
 */
static uint32_t ioapic_read(uint8_t reg) {
    g_ioapic[IOAPIC_REG_SEL / 4] = reg;
    return g_ioapic[IOAPIC_REG_WIN / 4];
}

/*
 * Nova Função Estendida Corrigida: Garante que desmascara o pino (Bit 16 = 0)
 * e injeta o perfil elétrico exato (Level/Low) exigido pelo silício do ICH9.
 */
void ioapic_set_irq_extended(uint8_t irq, uint64_t apic_id, uint8_t vector, uint8_t type) {
    uint8_t reg_low = IOAPIC_REDTBL + (irq * 2);
    uint8_t reg_high = reg_low + 1;

    // Bits 0..7: Vetor destino na IDT (Ex: 32 + irq)
    uint32_t low_flags = vector & 0xFF; 

    // CORREÇÃO CRÍTICA: Força o Bit 16 (Interrupt Mask) a ZERO para desmascarar e ativar o pino.
    // Também garante o Delivery Mode em 0 (Fixed) e Destination Mode em 0 (Physical).
    low_flags &= ~(1 << 16); 

    // Se type == 1 (PCI no ICH9/VMware/Real), configura o perfil elétrico obrigatório:
    if (type == 1) {
        low_flags |= (1 << 15); // Bit 15 = 1: Level Triggered (Disparado por Nível)
        low_flags |= (1 << 13); // Bit 13 = 1: Active Low (Sinal elétrico ativo em baixo)
    }
    else {
        // Se for 0 (ISA), garante Edge Triggered (Bit 15 = 0) e Active High (Bit 13 = 0)
        low_flags &= ~(1 << 15);
        low_flags &= ~(1 << 13);
    }

    // Parte Alta: Bits 56..63 recebem o Local APIC ID mapeado para a CPU alvo
    uint32_t high_flags = (apic_id & 0xFF) << 24;

    // Envia os comandos de escrita sincronizados para as janelas MMIO do chip
    ioapic_write(reg_low, low_flags);
    ioapic_write(reg_high, high_flags);
}

void ioapic_set_irq(uint8_t irq, uint64_t apic_id, uint8_t vector) {
    // 0 = Tipo padrão ISA (Edge Triggered, Active High)
    ioapic_set_irq_extended(irq, apic_id, vector, 0);
}

/*
 * Inicializa o chip IOAPIC mapeando a memória e desativando as IRQs legadas
 */
void ioapic_init(void) {
    kprintf("[IOAPIC] Mapeando registadores de IO em 0x%lX...\n", IOAPIC_PHYS_DEFAULT);

    g_ioapic = (volatile uint32_t *) vmm_map_device(IOAPIC_PHYS_DEFAULT, 4096);

    if (!g_ioapic) {
        kernel_panic("[IOAPIC ERRO CRITICO] Falha ao projetar MMIO do IOAPIC!\n");
        for(;;);
    }

    uint32_t version_reg = ioapic_read(IOAPIC_VER);
    uint32_t max_interrupts = ((version_reg >> 16) & 0xFF) + 1;

    kprintf("[IOAPIC] Chip detectado. Versao: 0x%X, Pinos de interrupcao suportados: %u\n", 
            version_reg & 0xFF, max_interrupts);

    for (uint32_t i = 0; i < max_interrupts; i++) {
        ioapic_write(IOAPIC_REDTBL + (i * 2), 0x10000);
        ioapic_write(IOAPIC_REDTBL + (i * 2) + 1, 0);
    }

    kprintf("[IOAPIC] Configuração global do barramento concluída com sucesso.\n");
}