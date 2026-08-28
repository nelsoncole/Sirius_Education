; Exemplo de entrada num Handler de Interrupção genérico no Sirius_Education
global interrupt_common_stub
interrupt_common_stub:
    ; 1. VERIFICAÇÃO SE A INTERRUPÇÃO VEIO DO USER MODE (RING 3)
    ; O seletor CS salvo pelo hardware no stack diz-nos o Ring anterior.
    ; O bit 0 e 1 do CS (offset 16 no stack após os pushes) indicam o CPL.
    
    test qword [rsp + 8], 3  ; Compara os bits de privilégio do CS salvo
    jz .skip_swapgs          ; Se já estava no Ring 0, pula o swapgs!

    swapgs                   ; Se veio do Ring 3, ativa a estrutura CpuDataBlock do Kernel
.skip_swapgs:

    ; 2. SALVA O CONTEXTO DOS REGISTADORES
    push rbp
    push rdi
    push rsi
    ; ... restantes pushes (rax, rbx, rcx, rdx, r8-r15) ...

    ; 3. CHAMA O C HANDLER CENTRAL
    mov rdi, rsp             ; Passa os registadores como argumento para a função C
    call interrupt_handler_c

    ; 4. RESTAURA O CONTEXTO DOS REGISTADORES
    ; ... pops de r15 até rax ...
    pop rsi
    pop rdi
    pop rdi
    pop rbp

    ; 5. DEVOLVE O GS DO UTILIZADOR ANTES DE SAIR
    test qword [rsp + 8], 3
    jz .skip_swapgs_exit
    swapgs                   ; Devolve o GS que o Ring 3 estava a usar
.skip_swapgs_exit:

    iretq                    ; Retorna da interrupção em modo de 64 bits
