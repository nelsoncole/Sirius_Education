; ============================================================================
;        Project: Sirius_Education
;       Filename: interrupt.asm
;    Description: Stubs de baixo nível em Assembly para tratamento de
;                 exceções e interrupções em Long Mode (x86_64).
;                 Garante alinhamento de pilha, tratamento de código de erro
;                 e chaveamento seguro Per-CPU via SWAPGS.
; 
;         Author: Nelson Cole
;   Created Date: 31/08/2026
; 
;    Modified By: Nelson Cole
;  Modified Date: 05/09/2026
; 
;        License: MIT
; ============================================================================

bits 64
section .text

extern interrupt_handler_c
global interrupt_common_stub

; Exporta o rótulo de saída para permitir comutação voluntária no SYS_EXIT
global interrupt_exit_stub

; ============================================================================
; MACROS PARA GERAR OS HANDLERS INDIVIDUAIS DE EXCEÇÃO
; ============================================================================

%macro ISR_NO_ERR_CODE 1
global isr%1
isr%1:
    push qword 0         ; Injeta código de erro falso de 8 bytes
    push qword %1        ; Injeta o número do vetor da exceção
    jmp interrupt_common_stub
%endmacro

%macro ISR_ERR_CODE 1
global isr%1
isr%1:
    push qword %1        ; Injeta apenas o número do vetor da exceção
    jmp interrupt_common_stub
%endmacro

; ============================================================================
; DEFINIÇÃO DAS 32 EXCEÇÕES NATIVAS DA INTEL/AMD
; ============================================================================
ISR_NO_ERR_CODE 0  ; #DE: Division by Zero
ISR_NO_ERR_CODE 1  ; #DB: Debug
ISR_NO_ERR_CODE 2  ; Non-Maskable Interrupt
ISR_NO_ERR_CODE 3  ; #BP: Breakpoint
ISR_NO_ERR_CODE 4  ; #OF: Overflow
ISR_NO_ERR_CODE 5  ; #BR: Bound Range Exceeded
ISR_NO_ERR_CODE 6  ; #UD: Invalid Opcode
ISR_NO_ERR_CODE 7  ; #NM: Device Not Available
ISR_ERR_CODE    8  ; #DF: Double Fault
ISR_NO_ERR_CODE 9  ; Coprocessor Segment Overrun
ISR_ERR_CODE    10 ; #TS: Invalid TSS
ISR_ERR_CODE    11 ; #NP: Segment Not Present
ISR_ERR_CODE    12 ; #SS: Stack-Segment Fault
ISR_ERR_CODE    13 ; #GP: General Protection Fault
ISR_ERR_CODE    14 ; #PF: Page Fault
ISR_NO_ERR_CODE 15 ; Reserved
ISR_NO_ERR_CODE 16 ; #MF: x87 Floating-Point Exception
ISR_ERR_CODE    17 ; #AC: Alignment Check
ISR_NO_ERR_CODE 18 ; #MC: Machine Check
ISR_NO_ERR_CODE 19 ; #XM: SIMD Floating-Point Exception
ISR_NO_ERR_CODE 20 ; #VE: Virtualization Exception
ISR_ERR_CODE    21 ; Control Protection Exception
ISR_NO_ERR_CODE 22 ; Reserved
ISR_NO_ERR_CODE 23 ; Reserved
ISR_NO_ERR_CODE 24 ; Reserved
ISR_NO_ERR_CODE 25 ; Reserved
ISR_NO_ERR_CODE 26 ; Reserved
ISR_NO_ERR_CODE 27 ; Reserved
ISR_NO_ERR_CODE 28 ; Reserved
ISR_NO_ERR_CODE 29 ; Reserved
ISR_ERR_CODE    30 ; Security Exception
ISR_NO_ERR_CODE 31 ; Reserved

; ============================================================================
; VETORES EXCLUSIVOS DO CONTROLADOR DE INTERRUPÇÕES (LAPIC)
; ============================================================================
ISR_NO_ERR_CODE 32  ; isr32:  Handler do LAPIC Timer (O Relógio Mestre)
ISR_NO_ERR_CODE 254 ; isr254: Handler de Erros Internos do LAPIC
ISR_NO_ERR_CODE 255 ; isr255: Handler de Interrupções Espúrias do LAPIC

; ============================================================================
; STUB CENTRAL DE PRESERVAÇÃO E CHAVEAMENTO DE CONTEXTO
; ============================================================================
interrupt_common_stub:
    ; 1. VERIFICAÇÃO SE VEIO DO USER MODE (RING 3)
    test qword [rsp + 24], 3  ; Verifica os bits CS empilhados pelo hardware
    jz .skip_swapgs           ; Se for 0 (Kernel), salta o swapgs

    swapgs                    ; Ativa a GS_BASE do Kernel (CpuDataBlock)
.skip_swapgs:

    ; 2. SALVA O CONTEXTO DE REGISTRADORES GERAIS
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rax
    push rbx
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 3. CORREÇÃO DE ALINHAMENTO DA PILHA PARA A ABI DO GCC
    mov rbp, rsp
    and rsp, ~0xF             ; Garante alinhamento estrito de 16 bytes para funções C

    ; 4. PASSA O CONTEXTO COMO ARGUMENTO E CHAMA O GESTOR EM C
    mov rdi, rbp              ; RDI = Ponteiro registers_t*
    call interrupt_handler_c  ; O C processa e retorna o novo RSP em RAX

    ; 5. CHAVEAMENTO FÍSICO SEGURO DE CONTEXTO
    mov rsp, rax              ; Substitui a pilha atual pela pilha escolhida pelo scheduler

; ============================================================================
; PONTO DE RESTAURAÇÃO EXCLUSIVO PARA DESVIO DE VOLUNTÁRIOS
; ============================================================================
interrupt_exit_stub:

    ; 6. RESTAURA O CONTEXTO DA NOVA TAREFA
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbx                   
    pop rax
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp

    ; Remove o Número do Vetor e o Código de Erro (+16 bytes)
    add rsp, 16

    ; 7. DEVOLVE O GS DO UTILIZADOR SE A NOVA THREAD FOR DE RING 3
    test qword [rsp + 8], 3   
    jz .skip_swapgs_exit
    swapgs                    ; Restaura a GS_BASE do utilizador para o Ring 3
.skip_swapgs_exit:

    iretq                     ; Retorno atómico