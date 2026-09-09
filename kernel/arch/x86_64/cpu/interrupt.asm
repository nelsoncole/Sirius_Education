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
ISR_NO_ERR_CODE 254 ; isr254: Handler de Erros Internos do LAPIC
ISR_NO_ERR_CODE 255 ; isr255: Handler de Interrupções Espúrias do LAPIC

; ============================================================================
; VETORES DO CONTROLADOR DE INTERRUPÇÕES (LAPIC HARDWARE)
; ============================================================================
ISR_NO_ERR_CODE 32  ; isr32:  Handler do LAPIC Timer (O Relógio Mestre)

; ============================================================================
; VETORES DO CONTROLADOR DE INTERRUPÇÕES IRQS DE HARDWARE EXTERNAS (Roteadas via IOAPIC)
; ============================================================================
ISR_NO_ERR_CODE 33  ; isr33:  IRQ 1 - Teclado PS/2 (Mapeado de forma padrão)
ISR_NO_ERR_CODE 34  ; isr34:  IRQ 2 - Cascata (PIC/APIC interno)
ISR_NO_ERR_CODE 35  ; isr35:  IRQ 3 - Porta Série UART2
ISR_NO_ERR_CODE 36  ; isr36:  IRQ 4 - Porta Série UART1
ISR_NO_ERR_CODE 37  ; isr37:  IRQ 5 - Placa de Som / Paralela
ISR_NO_ERR_CODE 38  ; isr38:  IRQ 6 - Controlador de Disquetes
ISR_NO_ERR_CODE 39  ; isr39:  IRQ 7 - Porta Paralela
ISR_NO_ERR_CODE 40  ; isr40:  IRQ 8 - Real Time Clock (RTC)
ISR_NO_ERR_CODE 41  ; isr41:  IRQ 9 - Redirecionamento ACPI
ISR_NO_ERR_CODE 42  ; isr42:  IRQ 10 - Periféricos PCI / Controladores USB
ISR_NO_ERR_CODE 43  ; isr43:  IRQ 11 - Periféricos PCI / Rede / AHCI
ISR_NO_ERR_CODE 44  ; isr44:  IRQ 12 - Rato PS/2 (Mouse)
ISR_NO_ERR_CODE 45  ; isr45:  IRQ 13 - Coprocessador Matemático
ISR_NO_ERR_CODE 46  ; isr46:  IRQ 14 - Disco Rígido ATA/IDE / AHCI Primário
ISR_NO_ERR_CODE 47  ; isr47:  IRQ 15 - Disco Rígido ATA/IDE Secundário
ISR_NO_ERR_CODE 48  ; isr48:  GSI 16 - Barramento PCIe Slot 1 (Ex: GPU Emulada)
ISR_NO_ERR_CODE 49  ; isr49:  GSI 17 - Barramento PCIe Slot 2 (Ex: Controladores xHCI)
ISR_NO_ERR_CODE 50  ; isr50:  GSI 18 - Barramento PCIe / Canais SMBus adicionais
ISR_NO_ERR_CODE 51  ; isr51:  GSI 19 - Barramento PCIe / Reservado Hardware
ISR_NO_ERR_CODE 52  ; isr52:  GSI 20 - Barramento PCIe / Som de Alta Definição (HDA)
ISR_NO_ERR_CODE 53  ; isr53:  GSI 21 - Barramento PCIe / Dispositivos VirtIO adicionais
ISR_NO_ERR_CODE 54  ; isr54:  GSI 22 - Barramento PCIe / Pontes secundárias
ISR_NO_ERR_CODE 55  ; isr55:  GSI 23 - Barramento PCIe / Interfaces USB adicionais
ISR_NO_ERR_CODE 56  ; isr56:  GSI 24 - Alocação Dinâmica Chipset / ACPI PCI Routing
ISR_NO_ERR_CODE 57  ; isr57:  GSI 25 - Alocação Dinâmica Chipset
ISR_NO_ERR_CODE 58  ; isr58:  GSI 26 - Alocação Dinâmica Chipset
ISR_NO_ERR_CODE 59  ; isr59:  GSI 27 - Alocação Dinâmica Chipset
ISR_NO_ERR_CODE 60  ; isr60:  GSI 28 - Alocação Dinâmica Chipset
ISR_NO_ERR_CODE 61  ; isr61:  GSI 29 - Alocação Dinâmica Chipset
ISR_NO_ERR_CODE 62  ; isr62:  GSI 30 - Alocação Dinâmica Chipset
ISR_NO_ERR_CODE 63  ; isr63:  GSI 31 - Alocação Dinâmica Chipset
ISR_NO_ERR_CODE 64  ; isr64:  GSI 32 - Extensão MSI-X Mapeada por Hardware
ISR_NO_ERR_CODE 65  ; isr65:  GSI 33 - Extensão MSI-X
ISR_NO_ERR_CODE 66  ; isr66:  GSI 34 - Extensão MSI-X
ISR_NO_ERR_CODE 67  ; isr67:  GSI 35 - Extensão MSI-X
ISR_NO_ERR_CODE 68  ; isr68:  GSI 36 - Extensão MSI-X
ISR_NO_ERR_CODE 69  ; isr69:  GSI 37 - Extensão MSI-X
ISR_NO_ERR_CODE 70  ; isr70:  GSI 38 - Extensão MSI-X
ISR_NO_ERR_CODE 71  ; isr71:  GSI 39 - Extensão MSI-X
ISR_NO_ERR_CODE 72  ; isr72:  GSI 40 - Extensão Placas Multi-Dispositivo
ISR_NO_ERR_CODE 73  ; isr73:  GSI 41 - Extensão Placas Multi-Dispositivo
ISR_NO_ERR_CODE 74  ; isr74:  GSI 42 - Extensão Placas Multi-Dispositivo
ISR_NO_ERR_CODE 75  ; isr75:  GSI 43 - Extensão Placas Multi-Dispositivo
ISR_NO_ERR_CODE 76  ; isr76:  GSI 44 - Extensão Placas Multi-Dispositivo
ISR_NO_ERR_CODE 77  ; isr77:  GSI 45 - Extensão Placas Multi-Dispositivo
ISR_NO_ERR_CODE 78  ; isr78:  GSI 46 - Extensão Placas Multi-Dispositivo
ISR_NO_ERR_CODE 79  ; isr79:  GSI 47 - Extensão Placas Multi-Dispositivo / Teto Máximo
ISR_NO_ERR_CODE 80  ; isr80:  Vetor de Guarda de Transbordo (Fim da Janela do IOAPIC)

; ============================================================================
; JANELA NATIVA DE VETORES MSI (MAPEADOS DE 81 A 112)
; ============================================================================
ISR_NO_ERR_CODE 81  ; msi0:  Corresponde ao índice 0 da fnvetors_handler_msi (ex: AHCI)
ISR_NO_ERR_CODE 82  ; msi1:  Índice 1
ISR_NO_ERR_CODE 83  ; msi2
ISR_NO_ERR_CODE 84  ; msi3
ISR_NO_ERR_CODE 85  ; msi4
ISR_NO_ERR_CODE 86  ; msi5
ISR_NO_ERR_CODE 87  ; msi6
ISR_NO_ERR_CODE 88  ; msi7
ISR_NO_ERR_CODE 89  ; msi8
ISR_NO_ERR_CODE 90  ; msi9
ISR_NO_ERR_CODE 91  ; msi10
ISR_NO_ERR_CODE 92  ; msi11
ISR_NO_ERR_CODE 93  ; msi12
ISR_NO_ERR_CODE 94  ; msi13
ISR_NO_ERR_CODE 95  ; msi14
ISR_NO_ERR_CODE 96  ; msi15
ISR_NO_ERR_CODE 97  ; msi16
ISR_NO_ERR_CODE 98  ; msi17
ISR_NO_ERR_CODE 99  ; msi18
ISR_NO_ERR_CODE 100 ; msi19
ISR_NO_ERR_CODE 101 ; msi20
ISR_NO_ERR_CODE 102 ; msi21
ISR_NO_ERR_CODE 103 ; msi22
ISR_NO_ERR_CODE 104 ; msi23
ISR_NO_ERR_CODE 105 ; msi24
ISR_NO_ERR_CODE 106 ; msi25
ISR_NO_ERR_CODE 107 ; msi26
ISR_NO_ERR_CODE 108 ; msi27
ISR_NO_ERR_CODE 109 ; msi28
ISR_NO_ERR_CODE 110 ; msi29
ISR_NO_ERR_CODE 111 ; msi30
ISR_NO_ERR_CODE 112 ; msi31 : Canal máximo alocado no subsistema msi.c


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