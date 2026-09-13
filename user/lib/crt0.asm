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

; Força o posicionamento deste bloco no topo absoluto do binário (.text.boot)
section .text.boot

global _start
extern main

_start:
    ; 1. LIMPEZA DE REGISTADORES DE CONTROLO
    ; Limpamos o RBP para indicar aos depuradores o fim do Backtrace da Stack.
    xor rbp, rbp

    ; ============================================================================
    ; 2. EXTRAÇÃO DE ARGUMENTOS DA PILHA (System V ABI AMD64 Compliance)
    ;
    ; No momento do arranque do processo, a pilha está estruturada assim:
    ; [rsp]     = argc (inteiro de 64-bits)
    ; [rsp + 8] = argv (ponteiro para o array de strings)
    ; ============================================================================
    
    mov rdi, [rsp]              ; RDI = 1º Argumento de C: argc (Carrega o valor de [rsp])
    lea rsi, [rsp + 8]          ; RSI = 2º Argumento de C: argv (Endereço do array de strings)

    ; 3. ALINHAMENTO DA PILHA A 16 BYTES ANTES DO CALL
    ; A ABI exige que o RSP esteja alinhado a 16 bytes antes de qualquer call.
    ; Como o sysret não empurra o RIP/CS para a pilha, o RSP já está alinhado.
    ; Fazemos um and protetivo apenas se necessário, mas a extração direta basta:
    
    ; 4. SALTO PARA A APLICAÇÃO (Função main da Shell)
    ; A assinatura em C será: int main(int argc, char* argv[]);
    call main

    ; ============================================================================
    ; 5. ENCERRAMENTO AUTOMÁTICO VIA SYSCALL
    ; Se a função main() retornar voluntariamente, o seu valor de retorno 
    ; estará guardado em RAX. Capturamos esse valor e disparamos a SYS_EXIT.
    ; ============================================================================
    mov rdi, rax                ; RDI = Código de status retornado pelo main() (1º param)
    mov rax, 3                  ; RAX = 3 -> Número lógico da SYS_EXIT (syscall.h)
    
    ; Transição física para o seu 'syscall_entry_stub' no Kernel!
    syscall                     

    ; Linha de salvaguarda extrema (Dead Code)
.halt:
    hlt
    jmp .halt