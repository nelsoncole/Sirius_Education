/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: reg.h
 *    Description: Estrutura de espelhamento do estado dos registadores da 
 *                 CPU x86_64 salvos durante uma interrupção ou exceção.
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

#ifndef _REG_H_
#define _REG_H_

/*
 * Espelho exato da pilha de registadores empurrada pelo interrupt.asm.
 * O atributo packed garante que o compilador não insira paddings, casando
 * byte a byte com o layout do hardware e dos stubs em Assembly.
 */
typedef struct {
    // Registadores gerais salvos manualmente pelo stub comum (Ordem inversa dos pops)
    unsigned long r15, r14, r13, r12, r11, r10, r9, r8;
    unsigned long rbx, rax, rcx, rdx, rsi, rdi, rbp;

    // Metadados injetados pelas macros de interrupção
    unsigned long int_no;      // Número do vetor da interrupção (0 a 255)
    unsigned long error_code;  // Código de erro nativo da CPU ou 0 falso

    // Contexto de execução salvo automaticamente pelo hardware do processador (x86_64)
    unsigned long rip;
    unsigned long cs;
    unsigned long rflags;
    unsigned long rsp;         // Pilha ativa no momento da interrupção
    unsigned long ss;
} __attribute__((packed)) registers_t;

#endif // _REG_H_
