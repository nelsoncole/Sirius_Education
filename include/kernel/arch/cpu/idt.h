/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: gdt.h
 *    Description: Estrutura da tabela IDT e IDTR
 * 
 *         Author: Nelson Cole
 *   Created Date: 28/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 29/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _IDT_H_
#define _IDT_H_

typedef struct _idt{

    unsigned long long offset_15_0 :16; 
	unsigned long long sel : 16;
	unsigned long long ist:3;
	unsigned long long unused :5;
	unsigned long long type : 5;
	unsigned long long dpl : 2;
    unsigned long long p :1;
	unsigned long long offset_31_16 : 16;
	unsigned long long offset_63_32 : 32;
	unsigned long long reserved : 32;

}__attribute__ ((packed)) idt_t;

typedef struct _idtr{
    unsigned short	limit;
	unsigned long  base;

}__attribute__((packed)) idtr_t;


#endif
