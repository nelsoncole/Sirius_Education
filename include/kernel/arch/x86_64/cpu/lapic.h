/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: lapic.h
 *    Description: Definições de registadores de hardware e protótipos 
 *                 para a inicialização do Local APIC (LAPIC).
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

#ifndef _LAPIC_H_
#define _LAPIC_H_

#include <kernel/lib/stdint.h>

#define LAPIC_PHYS_DEFAULT      0xFEE00000UL    // Endereço físico padrão do LAPIC

/* 
 * REGISTADORES CRÍTICOS DO LAPIC (Offsets de MMIO)
 */
#define LAPIC_REG_ID            0x0020          // ID do Local APIC (Bits 24..31)
#define LAPIC_REG_VERSION       0x0030          // Versão do LAPIC
#define LAPIC_REG_EOI           0x00B0          // End of Interrupt (Escrita limpa a interrupção)
#define LAPIC_REG_SVR           0x00F0          // Spurious Interrupt Vector Register
#define LAPIC_REG_ESR           0x0280          // Error Status Register (Registo de Estado de Erros)
#define LAPIC_REG_ICR_LOW       0x0300          // Interrupt Command Register Baixo (Para enviar IPIs)
#define LAPIC_REG_ICR_HIGH      0x0310          // Interrupt Command Register Alto
#define LAPIC_REG_LVT_TIMER     0x0320          // Registador LVT do Temporizador Local
#define LAPIC_REG_LVT_ERROR     0x0370          // Registador LVT de Erros Internos do Chip

/* REGISTADORES AUXILIARES DE CALIBRAÇÃO DO TIMOR LOCAL */
#define LAPIC_REG_TICR          0x0380          // Timer Initial Count Register (Contador Inicial)
#define LAPIC_REG_TCCR          0x0390          // Timer Current Count Register (Contador Atual)
#define LAPIC_REG_TDCR          0x03E0          // Timer Divide Configuration Register (Divisor)

/* Protótipos Globais */
void          lapic_init(void);
void          lapic_eoi(void);
unsigned int  lapic_get_id(void);
void          lapic_timer_init(uint32_t hz);

/* Função de barramento para comunicação Inter-Processor (IPI) no SMP */
void          lapic_write_reg(uint32_t offset, uint32_t value);
uint32_t      lapic_read_reg(uint32_t offset);

#endif // _LAPIC_H_
