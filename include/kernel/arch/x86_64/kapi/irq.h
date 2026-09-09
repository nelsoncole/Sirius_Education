/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: irq.h
 *    Description: Interface de Abstração de Hardware (KAPI) para gestão e
 *                 registo de rotinas de interrupção (IRQs).
 *                 Mapeia o suporte estendido ao controlador IOAPIC moderno.
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

#ifndef _IRQ_H_
#define _IRQ_H_

#include <kernel/lib/stdint.h>

/* Linha física padrão do barramento para periféricos comuns */
#define IRQ_KEYBOARD        1   /* O Teclado PS/2 está mapeado obrigatoriamente na IRQ 1 */

/* Limite estendido do hardware para suportar as 48 GSIs do IOAPIC moderno */
#define MAX_IOAPIC_PINS     48

/* Definição do protótipo que o handler de interrupção do driver deve seguir */
typedef void (*irq_handler_t)(void);

/* 
 * Vetor global estendido exportado para o Kernel. 
 * Marcado como extern para ser inicializado em irq.c e consumido livremente em isr.c.
 */
extern irq_handler_t g_interrupt_handlers[MAX_IOAPIC_PINS];

/**
 * Regista uma função de callback (handler) associada a uma linha de IRQ específica.
 * 
 * @param irq_number Número da interrupção física de hardware (ex: 1 para Teclado).
 * @param handler    Ponteiro para a função que processará o evento.
 * @return 0 em caso de sucesso, ou erro negativo.
 */
int kapi_register_irq_handler(uint8_t irq_number, irq_handler_t handler);

/**
 * Desmascara (habilita) a linha de interrupção no controlador IOAPIC.
 * 
 * @param irq_number Linha de interrupção a ser ativada (0 a 47).
 */
void kapi_enable_irq(uint8_t irq_number);

/**
 * Mascara (desabilita) uma linha de interrupção física no IOAPIC.
 * 
 * @param irq_number Linha de interrupção a ser desativada.
 */
void kapi_disable_irq(uint8_t irq_number);

#endif /* _IRQ_H_ */