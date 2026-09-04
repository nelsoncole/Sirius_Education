/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ioapic.h
 *    Description: Estruturas, offsets e protótipos de hardware para
 *                 configuração e roteamento do chip IOAPIC.
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

#ifndef _IOAPIC_H_
#define _IOAPIC_H_

#include <kernel/lib/stdint.h>

#define IOAPIC_PHYS_DEFAULT     0xFEC00000UL    // Endereço físico padrão do IOAPIC

/*
 * REGISTADORES DE ACESSO VIA MMIO
 */
#define IOAPIC_REG_SEL          0x00            // I/O Register Select (32 bits)
#define IOAPIC_REG_WIN          0x10            // I/O Window Register (32 bits)

/*
 * REGISTADORES INTERNOS SELECIONADOS VIA IOREGSEL
 */
#define IOAPIC_ID               0x00            // ID do chip IOAPIC
#define IOAPIC_VER              0x01            // Versão e número máximo de redirecionamentos
#define IOAPIC_REDTBL           0x10            // Início da Tabela de Redirecionamento (2 entradas de 32 bits por pino)

/* Protótipos Globais */
void ioapic_init(void);
void ioapic_set_irq(uint8_t irq, uint64_t apic_id, uint8_t vector);

#endif // _IOAPIC_H_
