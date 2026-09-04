; ============================================================================
;        Project: Sirius_Education
;       Filename: trampoline.asm
;    Description: Ponto de entrada de baixo nível para os APs (Application Processors)
;                 Carrega a Paginação, muda para a stack exclusiva do Kernel e salta
;                 para a lógica em C.
; 
;         Author: Nelson Cole
;   Created Date: 31/08/2026
; 
;    Modified By: Nelson Cole
;  Modified Date: 04/09/2026
; 
;        License: MIT
; ============================================================================

org 0x8000
bits 16
    jmp 0x0000:boot_ap
times 0x08 db 0

; ============================================================================
; MAPA DE RESERVA FÍSICA DE MEMÓRIA (Apenas preenchimento limpo)
; ============================================================================
times 0x08 db 0   ; 0x8008 - Espaço reservado para o CR3 Mestre (8 bytes)
times 0x10 db 0   ; 0x8010 - Espaço reservado para a Stack Real (8 bytes)
times 0x18 db 0   ; 0x8018 - Espaço reservado para o CPU ID (4 bytes)
times 0x1C db 0   ; 0x801C - Espaço reservado para o LAPIC ID (4 bytes)
times 0x20 db 0   ; 0x8020 - Espaço reservado para o C Handler (8 bytes)

; Garante o isolamento completo do código executável a partir de 0x8040
times 0x40 db 0

boot_ap:
    cli
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Carrega GDT 32-bits local
    db 0x66		; Execute GDT 32-Bits
	lgdt [gdt32_pointer]
    	
        
    mov eax, cr0
    or  eax, 0x10001
    mov cr0, eax
    
    ; Salto para Modo Protegido de 32 bits
    jmp 0x08:start32            

align 16
bits 32
start32:
    mov ax, 0x10                
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x7C00

    lgdt [gdt64_pointer]        ; Carrega a GDT de 64-bits

    mov eax, cr4
    or  eax, 0xA0               ; PAE + PGE
    mov cr4, eax
        
    ; LEITURA DIRETA E FIXA: Carrega o CR3 diretamente do offset físico 0x8008
    mov eax, [0x8008]
    mov cr3, eax
        
    mov ecx, 0xC0000080
    rdmsr
    or  eax, 0x100              ; LME (Long Mode Enable)
    wrmsr

    ; HABILITAR NX
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 11             ; Ativa o bit 11 (NXE) no registador EAX
    wrmsr

    
    mov eax, cr0
    or  eax, 0x80000000         ; Ativa Paginação (O processador entra em Modo de Compatibilidade)
    mov cr0, eax

    ; Fazemos um far jump nativo de 32-bits para forçar o processador a entrar 
    ; no modo longo através do rótulo de 64-bits.
    jmp 0x08:start64            

; ============================================================================
; ENTRADA EM MODO LONGO NATIVO E SEGURO (64 BITS)
; ============================================================================
bits 64
default abs
align 16
start64:
    ; --- O AP ESTÁ AGORA EM MODO LONGO NATIVO E SEGURO ---

    ; 1. Configurar os registadores de segmento de dados de 64 bits para o Kernel
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; --------------------------------------------------------
    ; Preparar stack
    ; --------------------------------------------------------
    mov rsp, [0x8010]

    ; Stack deve estar alinhada a 16 bytes
    and rsp, -16

    ; ============================================================================
    ; BLINDAGEM CONTRA O GCC 15: ATIVAÇÃO OBRIGATÓRIA DE SSE / FPU
    ; ============================================================================
    mov     rax, cr4
    or      rax, 0x600          ; Ativa OSFXSR (bit 9) e OSXMMEXCPT (bit 10)
    mov     cr4, rax

    mov     rax, cr0
    and     ax,  0xFFFB         ; Garante bit EM (Emulação) desativado
    or      rax, 0x2            ; Garante bit MP (Monitor Coprocessor) ativo
    mov     cr0, rax

    fninit                      ; Reseta e limpa o estado da FPU por hardware
    ; ============================================================================

    ; System V ABI (RDI, RSI, RDX, RCX, ...)
    mov rdi, [0x8018]           ; cpu_id
    mov rsi, [0x801C]           ; lapic_id
    mov rdx, [0x8010]           ; stack_top

    ; 3. SALTO DEFINITIVO PARA A FUNÇÃO EM C NA MEMÓRIA ALTA
    mov rax, [0x8020]           ; c_handler
    jmp rax                     ; O AP entra no Kernel com suporte total a código C moderno

; ============================================================================
; TABELAS GDT LOCALIZADAS NO FIM DO BINÁRIO
; ============================================================================
align 16
gdt32:
    dq 0x0000000000000000       
    dq 0x00cf9a000000ffff       ; Seletor 0x08: Code 32
    dq 0x00cf92000000ffff       ; Seletor 0x10: Data 32
gdt32_end:

gdt32_pointer:
    dw (gdt32_end - gdt32) - 1
    dd gdt32                    ; Endereço físico de 32-bits

align 16
gdt64:
    dq 0x0000000000000000       
    dq 0x00209a0000000000       ; Seletor 0x08: Code 64 (Bit L ativo)
    dq 0x0000920000000000       ; Seletor 0x10: Data 64
gdt64_end:

gdt64_pointer:
    dw (gdt64_end - gdt64) - 1
    dd gdt64                    ; Forçado DD para leitura correta em 32-bits
