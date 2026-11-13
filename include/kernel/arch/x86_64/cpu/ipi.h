/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ipi.h
 *    Description: Definições de vetores e protótipos para comunicação e
 *                 sincronização entre processadores via IPI
 *                 (Inter-Processor Interrupts).
 * 
 *         Author: Nelson Cole
 *   Created Date: 12/11/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 12/11/2026
 * 
 *        License: MIT
 * ============================================================================
 */
#ifndef _IPI_H_
#define _IPI_H_

#define IPI_FLUSH_TLB 0xF0

void handle_ipi_flush_tlb(void);
void smp_flush_tlb_all(void);


#endif
