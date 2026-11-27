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
 *  Modified Date: 26/11/2026
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
#define MAX_HANDLERS_PER_IRQ 8

typedef struct
{
    irq_handler_t handlers[MAX_HANDLERS_PER_IRQ];
    uint8_t count;
} irq_shared_handler_t;

static irq_shared_handler_t g_irq_handlers[MAX_IOAPIC_PINS];


/**
 * Regista uma função de tratamento (callback) associada a uma linha de IRQ/GSI.
 */
int irq_handler(uint8_t irq_number, irq_handler_t handler)
{
    if (irq_number >= MAX_IOAPIC_PINS || !handler)
        return -1;

    irq_shared_handler_t *entry = &g_irq_handlers[irq_number];

    if (entry->count >= MAX_HANDLERS_PER_IRQ)
    {
        kprintf("[IRQ Core] Limite de handlers atingido para GSI %u.\n",
                irq_number);
        return -1;
    }

    for (uint8_t i = 0; i < entry->count; i++)
    {
        if (entry->handlers[i] == handler)
            return 0;
    }

    entry->handlers[entry->count++] = handler;

    if (entry->count == 1)
        enable_irq(irq_number);

    return 0;
}

void irq_dispatch(uint8_t irq_number)
{
    if (irq_number >= MAX_IOAPIC_PINS)
        return;

    irq_shared_handler_t *entry = &g_irq_handlers[irq_number];

    for (uint8_t i = 0; i < entry->count; i++)
    {
        irq_handler_t handler = entry->handlers[i];

        if (handler)
            handler();
    }
}

/**
 * Habilita e configura uma linha de interrupção física no IOAPIC.
 */
void enable_irq(uint8_t irq_number)
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
void disable_irq(uint8_t irq_number)
{
    if (irq_number >= MAX_IOAPIC_PINS) return;

    uint8_t target_vector = IRQ_BASE_VECTOR + irq_number;
    uint32_t masked_flags = (1 << 16) | target_vector; 
    
    ioapic_set_irq(irq_number, 0, masked_flags);
}

void kapi_register_irq_handler(pci_device_t *dev, void (*func)(void))
{
    if (!dev || !func)
        return;

    if (apic_send_msi(dev, func) == 0)
    {
        kprintf("[KAPI] Dispositivo PCI %x:%x registado via MSI.\n",
                dev->vendor_id, dev->device_id);
        return;
    }

    int ioapic_pin = pci_route_ioapic_irq(dev);

    if (ioapic_pin < 0 || ioapic_pin >= MAX_IOAPIC_PINS)
    {
        kprintf("[KAPI] GSI invalido para dispositivo PCI %x:%x.\n",
                dev->vendor_id, dev->device_id);
        return;
    }

    kprintf("[KAPI] MSI nao suportado. Registando handler no GSI: %d\n",
            ioapic_pin);

    if (irq_handler((uint8_t)ioapic_pin, func) != 0)
    {
        kprintf("[KAPI] Falha ao registar handler no GSI %d.\n",
                ioapic_pin);
    }
}