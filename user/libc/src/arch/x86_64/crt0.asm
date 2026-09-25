; ============================================================================
;        Project: Sirius_Education
;       Filename: crt0.asm
;    Description: C Runtime Entry Point (Ponto de Inicialização) para Ring 3.
;                 Prepara o ambiente do processo e extrai argc/argv da pilha.
; 
;         Author: Nelson Cole
;   Created Date: 13/09/2026
; 
;    Modified By: Nelson Cole
;  Modified Date: 13/09/2026
; 
;        License: MIT
; ============================================================================

[BITS 64]

global _start
extern main

_start:
    ; 1. LIMPEZA DE REGISTADORES DE CONTROLO
    xor rbp, rbp

    ; ============================================================================
    ; 2. EXTRAÇÃO DE ARGUMENTOS DA PILHA (System V ABI AMD64 Compliance)
    ; ============================================================================
    mov rdi, [rsp]              ; RDI = 1º Argumento de C: argc
    lea rsi, [rsp + 8]          ; RSI = 2º Argumento de C: argv

    ; ============================================================================
    ; 3. ALINHAMENTO ESTRITO DA PILHA A 16 BYTES (CORREÇÃO DE SEGURANÇA)
    ; Se o RSP inicial terminar em 0x8 (alinhado a 8 bytes), subtraímos 8 bytes
    ; para que fique alinhado a 16 bytes (terminado em 0x0) antes do CALL.
    ; ============================================================================
    and rsp, -16                ; Mascara os bits inferiores, alinhando a 16 bytes

    ; 4. SALTO PARA A APLICAÇÃO (Função main da Shell)
    call main

    ; ============================================================================
    ; 5. ENCERRAMENTO AUTOMÁTICO VIA SYSCALL
    ; ============================================================================
    mov rdi, rax                ; RDI = Código de status retornado pelo main()
    mov rax, 3                  ; RAX = 3 -> Número lógico da SYS_EXIT (SYS_EXIT no usyscall.h)
    
    syscall                     ; Transição física para o Kernel

.loop:
    pause
    jmp .loop