/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: panic.h
 *    Description: Definições e protótipos para o subsistema de tratamento
 *                 de falhas críticas do Kernel (Kernel Panic).
 * 
 *         Author: Nelson Cole
 *   Created Date: 30/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 30/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _PANIC_H_
#define _PANIC_H_

/*
 * INTERRUPÇÃO CONTROLADA DO SISTEMA (KERNEL PANIC)
 * ------------------------------------------------------------------------
 * Bloqueia o processamento em Ring 0, desativa interrupções de hardware,
 * emite um alerta descritivo via kprintf e congela a CPU em ciclo HLT.
 * 
 * @param message String de texto contendo a causa irrecuperável da falha.
 */
void kernel_panic(const char* message);

#endif /* _PANIC_H_ */
