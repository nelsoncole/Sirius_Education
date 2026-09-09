/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: irq.c
 *    Description: Subsistema Core de gestão de Handlers de interrupção.
 *                 Interage diretamente com o driver IOAPIC nativo do sistema
 *                 para registo e controlo de roteamento de hardware.
 * 
 *         Author: Nelson Cole
 *   Created Date: 07/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 07/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/kapi/irq.h>
#include <kernel/arch/x86_64/kapi/msi.h>
#include <kernel/arch/x86_64/cpu/ioapic.h>
#include <kernel/kernel/mm/memory_map.h>
#include <kernel/klib.h>

  
#define IRQ_BASE_VECTOR     0x21  /* Vetores começam em 33 na IDT (32 = LAPIC Timer) */

/* 
 * Vetor estendido indexado estritamente de 0 a 47 (Pinos do IOAPIC).
 * Consumido via técnica de subtração pela rotina central em isr.c.
 */
irq_handler_t g_interrupt_handlers[MAX_IOAPIC_PINS];


/**
 * Regista uma função de tratamento (callback) associada a uma linha de IRQ/GSI.
 */
int kapi_register_irq_handler(uint8_t irq_number, irq_handler_t handler)
{
    /* Validação defensiva contra transbordo de memória do pool de hardware */
    if (irq_number >= MAX_IOAPIC_PINS)
    {
        kprintf("[IRQ Core] Erro: Linha fisica IOAPIC %d excede as 48 suportadas.\n", irq_number);
        return -1;
    }

    g_interrupt_handlers[irq_number] = handler;

    kapi_enable_irq(irq_number);

    return 0;
}

/**
 * Habilita e configura uma linha de interrupção física no IOAPIC.
 */
void kapi_enable_irq(uint8_t irq_number)
{
    if (irq_number >= MAX_IOAPIC_PINS) return;

    uint8_t target_vector = IRQ_BASE_VECTOR + irq_number;
    uint64_t target_core_apic_id = 0; /* Roteamento default: BSP (Core 0) */

    /* Desvia e grava a configuração diretamente na tabela física via ioapic.c */
    ioapic_set_irq(irq_number, target_core_apic_id, target_vector);
}

/**
 * Mascara (silencia) uma linha de interrupção física no IOAPIC ativando o bit 16.
 */
void kapi_disable_irq(uint8_t irq_number)
{
    if (irq_number >= MAX_IOAPIC_PINS) return;

    uint8_t target_vector = IRQ_BASE_VECTOR + irq_number;
    uint32_t masked_flags = (1 << 16) | target_vector; 
    
    ioapic_set_irq(irq_number, 0, masked_flags);
}