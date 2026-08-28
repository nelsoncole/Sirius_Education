; ============================================================================
;        Project: Sirius_Education
;       Filename: entry.asm
;    Description: Ponto de entrada de baixo nível para a arquitetura x86_64.
;                 Configura a stack inicial e salta para o kernel_main.
; 
;         Author: Nelson Cole
;   Created Date: 25/08/2026
; 
;    Modified By: Nelson Cole
;  Modified Date: 27/08/2026
; 
;        License: MIT
; ============================================================================

bits 64

section .text.boot

global _start
extern kernel_main

; ============================================================
; Kernel Entry Point
; ============================================================

_start:
    ; --------------------------------------------------------
    ; Guardar os argumentos recebidos pelo bootloader
    ; --------------------------------------------------------
    ;
    ;   RDI / RCX = BootInfo* (Depende da ABI do bootloader)
    ;

    ; --------------------------------------------------------
    ; Preparar stack
    ; --------------------------------------------------------
    lea rsp, [rel stack_top]

    ; Stack deve estar alinhada a 16 bytes
    and rsp, -16

    ; --------------------------------------------------------
    ; Chamar o kernel principal
    ; --------------------------------------------------------
    ; Se o bootloader UEFI foi compilado em ambiente Windows/MinGW, 
    ; o primeiro argumento vem em RCX. Convertemos para System V ABI (RDI).
    mov rdi, rcx
    call kernel_main

    ; --------------------------------------------------------
    ; kernel_main não deve retornar
    ; --------------------------------------------------------
.hang:
    cli
    hlt
    jmp .hang

; ============================================================
; Kernel Stack
; ============================================================
section .bss
align 16

stack_bottom:
    resb 16384                ; 16 KiB

stack_top: