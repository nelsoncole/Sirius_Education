; ============================================================================
;        Project: Sirius_Education
;       Filename: entry.asm
;    Description: Ponto de entrada de baixo nível para a arquitetura x86_64.
;                 Configura uma GDT básica de boot, inicializa a stack inicial
;                 e salta para o kernel_main com os argumentos alinhados.
; 
;         Author: Nelson Cole
;   Created Date: 25/08/2026
; 
;    Modified By: Nelson Cole
;  Modified Date: 28/08/2026
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
    ;
    ; RDI / RCX = BootInfo* (Depende da ABI do bootloader)
    ; --------------------------------------------------------

    ; --------------------------------------------------------
    ; Forçar CLI por segurança (garantir interrupções desligadas)
    ; --------------------------------------------------------
    cli

    ; --------------------------------------------------------
    ; Carregar GDT Básica de Boot (Isola o Kernel da GDT do UEFI/BIOS)
    ; --------------------------------------------------------
    lgdt [rel gdtr]

    ; Recarregar o segmento de código (CS) usando um jmp longe (Far Jmp)
    ; Em 64 bits, isto faz o CPU aplicar o novo seletor de código (0x08)
    push 0x08               ; Novo seletor de código do Kernel
    lea rax, [rel .reload_segments]
    push rax
    retfq                   ; Far Return de 64 bits (atua como o far jmp)

.reload_segments:
    ; Recarregar os segmentos de dados (0x10)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

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
; GDT Estruturada de Boot (Apenas Nulo, Código e Dados)
; ============================================================
align 16
gdt_start:
    ; Seletor 0x00: Descriptor Nulo
    dq 0x0000000000000000 

    ; Seletor 0x08: Kernel Code (Exec/Read, Base 0, Limit 0, Long Mode ativo)
    ; Base e limite são ignorados em 64 bits, mas os bits de acesso (0x9A) são chave.
    dq 0x00209A0000000000 

    ; Seletor 0x10: Kernel Data (Read/Write, Base 0, Limit 0)
    ; Bits de acesso (0x92)
    dq 0x0000920000000000
gdt_end:

gdtr:
    dw gdt_end - gdt_start - 1   ; Limite da GDT
    dq gdt_start                 ; Endereço base da GDT

; ============================================================
; Kernel Stack
; ============================================================
section .bss
align 16

stack_bottom:
    resb 16384                ; 16 KiB

stack_top:
