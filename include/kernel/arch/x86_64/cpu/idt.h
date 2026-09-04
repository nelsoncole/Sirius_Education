/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: idt.h
 *    Description: Estrutura da tabela IDT e IDTR alinhada por bits para x86_64.
 * 
 *         Author: Nelson Cole
 *   Created Date: 28/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 31/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _IDT_H_
#define _IDT_H_

#include <kernel/lib/stdint.h>

#define IDT_MAX_ENTRIES 256

typedef struct _idt{
    // Primeira parte de 64 bits
    unsigned long long offset_15_0  : 16; // Bits 0..15 do endereço do Handler
    unsigned long long sel          : 16; // Seletor de segmento de código (0x08)
    unsigned long long ist          : 3;  // Índice da Interrupt Stack Table (TSS)
    unsigned long long unused       : 5;  // Reservado/Não usado
    unsigned long long type         : 5;  // Tipo de Gate (Ex: 0xE/11110b = 64-bit Interrupt Gate)
    unsigned long long dpl          : 2;  // Descriptor Privilege Level (0 = Kernel, 3 = User)
    unsigned long long p            : 1;  // Present bit (1 = Ativo)
    unsigned long long offset_31_16 : 16; // Bits 16..31 do endereço do Handler

    // Segunda parte de 64 bits
    unsigned long long offset_63_32 : 32; // Bits 32..63 do endereço do Handler
    unsigned long long reserved     : 32; // Reservado pela CPU (Sempre 0)

} __attribute__ ((packed)) idt_t;

typedef struct _idtr{
    unsigned short limit; // Tamanho da IDT - 1
    unsigned long  base;  // Endereço virtual base da tabela IDT

} __attribute__((packed)) idtr_t;

void idt_init(void);
void idt_set_gate(uint8_t vector, unsigned long handler, uint16_t selector, uint8_t type, uint8_t dpl, uint8_t ist_index);

#endif // _IDT_H_

