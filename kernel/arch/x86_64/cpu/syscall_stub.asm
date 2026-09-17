; ============================================================================
;        Project: Sirius_Education
;       Filename: syscall_stub.asm
;    Description: Ponto de entrada de baixo nível para as Chamadas de Sistema.
;                 Preserva o contexto de Ring 3, isola os registadores críticos
;                 garante alinhamento estrito de 16 bytes e retorna via sysretq.
; 
;         Author: Nelson Cole
;   Created Date: 05/09/2026
;
;    Modified By: Nelson Cole
;  Modified Date: 06/09/2026
;
;        License: MIT
; ============================================================================

[BITS 64]
section .text

extern syscall_dispatcher
global syscall_entry_stub

align 16
syscall_entry_stub:
    ; 1. ISOLAMENTO DE PRIVILÉGIOS E COMUTAÇÃO DE PILHA
    swapgs                      ; Ativa a GS_BASE do Kernel
    
    mov [gs:8], rsp             ; Guarda temporariamente o RSP do utilizador
    mov rsp, [gs:0]             ; Carrega o kernel_stack_top estável

    ; ============================================================================
    ; PRESERVAÇÃO E ISOLAMENTO FÍSICO (6 Pushes = 48 bytes - Alinhado a 16)
    ; ============================================================================
    mov r14, rcx                ; R14 = RIP real de retorno
    mov r15, r11                ; R15 = RFLAGS originais

    push r14                    ; 1. [rsp + 40] Salva o RIP real de retorno
    push r15                    ; 2. [rsp + 32] Salva as RFLAGS originais
    push rbp                    ; 3. [rsp + 24] Preserva o RBP
    push rbx                    ; 4. [rsp + 16] Preserva o RBX
    push r10                    ; 5. [rsp + 8]  Salva o R10 (Antigo RSP ou argumento)
    push qword 0                ; 6. [rsp + 0]  PADDING final de alinhamento de 16 bytes

    ; ============================================================================
    ; 2. CONVERSÃO DE ARGUMENTOS
    ;
    ; Mapeamento: Hardware: RAX, RDI, RSI, RDX
    ; System V ABI C:       RDI, RSI, RDX, RCX
    ; ============================================================================

    mov rcx, rdx                ; RDX (Arg3 User) -> RCX (4º Param C)
    mov rdx, rsi                ; RSI (Arg2 User) -> RDX (3º Param C)
    mov rsi, rdi                ; RDI (Arg1 User) -> RSI (2º Param C)
    mov rdi, rax                ; RAX (Nº Syscall) -> RDI (1º Param C)       

    call syscall_dispatcher     ; Chamada ao dispatcher C
    
    ; ============================================================================
    ; 3. RESTAURO DO CONTEXTO DE RING 3 (Ordem Inversa Estrita e Perfeita)
    ; ============================================================================
    add rsp, 8                  ; 6. Remove o padding de alinhamento
    pop r10                     ; 5. Restaura R10
    pop rbx                     ; 4. Restaura RBX
    pop rbp                     ; 3. Restaura RBP
    
    pop r11                     ; 2. Restaura as RFLAGS originais direto para R11 (Exigido pelo sysret)
    pop rcx                     ; 1. Restaura o RIP real de retorno direto para RCX (Exigido pelo sysret)
    
    ; A Stack do Kernel está agora completamente vazia e limpa!
    
    mov rsp, [gs:8]             ; Restaura o RSP legítimo do utilizador
    swapgs                      ; Devolve o GS_BASE original do utilizador

    ; ============================================================================
    ; NOTA TÉCNICA DE ARQUITETURA (LIMITAÇÃO DO NASM & OPCODE):
    ; 1. O NASM (sintaxe Intel) não reconhece o mnemónico 'sysretq' [6.1], que é nativo
    ;    do GAS (sintaxe AT&T). Se for usado 'sysretq', o NASM interpreta-o como
    ;    uma label (etiqueta), omitindo o retorno no binário e gerando Triple Fault.
    ;
    ; 2. Em modo de 64 bits, a instrução 'sysret' pura gera, por padrão, o operando
    ;    de 32 bits ('sysretd', opcode '0x0F, 0x07'), o que força um retorno inválido
    ;    para o Modo de Compatibilidade de 32-bits, disparando uma #GP.
    ;
    ; 3. Para efetuar o retorno supersónico em 64-bits completo (Long Mode), é
    ;    estritamente obrigatório usar o prefixo 'o64'. Isto força o NASM a emitir
    ;    o REX.W prefix (opcode '0x48, 0x0F, 0x07'), garantindo que a CPU carrega
    ;    o User CS e User SS corretos de 64-bits da GDT.
    ; ============================================================================
    o64 sysret                  ; Retorno forçado a 64-bits (0x48, 0x0F, 0x07). RIP=RCX e RFLAGS=R11

