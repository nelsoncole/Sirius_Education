/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: gdt.h
 *    Description: Estrutura da tabela TSS
 * 
 *         Author: Nelson Cole
 *   Created Date: 28/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 28/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef __TSS_H__
#define __TSS_H__

typedef struct _tss {
	unsigned int reserved;
	unsigned long long rsp0, rsp1, rsp2;
	unsigned int reserved2[2];
	unsigned long long ist[7]; // Interrupt Stack Table (IST 1 a 7)
	unsigned int reserved3[2];
	unsigned short reserved4;
	unsigned short io_map_base_addr;
	
}__attribute__((packed)) tss_t;

#endif
