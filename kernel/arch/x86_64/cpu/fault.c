/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: fault.c
 *    Description: Funções em C para processamento de exceções como #NM e #PF de Pilha de Ring3.
 *
 *         Author: Nelson Cole
 *   Created Date: 17/09/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 17/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/klib.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/kvmm.h>
#include <kernel/kernel/mm/pmm.h>


/* Flags x86_64: Presente (0x1) | Read-Write (0x2) | User-Supervisor (0x4) */
#define PAGE_USER_FLAGS         (0x1 | 0x2 | 0x4)

/**
 * @brief Tratador de Hardware para a Exceção #NM (Vetor 7 - Device Not Available).
 *        Gerencia o intercâmbio de contexto Lazy FPU/SSE entre tarefas.
 */
void handle_device_not_available_exception(void) 
{
    // 1. Limpa o bit TS no CR0 imediatamente
    arch_fpu_clear_ts();

    cpu_data_block_t* cpu = get_current_cpu();
    thread_t* current = cpu->current_thread;

    if (!current) return;

    // 2. SALVAGUARDA: Se outra thread era a dona física do coprocessador, guarda o estado dela
    if (cpu->fpu_owner_thread != NULL && cpu->fpu_owner_thread != current) 
    {
        thread_t* old_owner = cpu->fpu_owner_thread;
        
        uint64_t addr = (uint64_t)old_owner->fpu_state;
        __asm__ __volatile__("fxsave64 (%0)" :: "r"(addr) : "memory");
    }

    // 3. RESTAURAÇÃO: Passa a posse do hardware matemático para a thread atual
    if (cpu->fpu_owner_thread != current) 
    {
        uint64_t addr = (uint64_t)current->fpu_state;

        // Validação da flag hexadecimal estável que corrigimos para o GCC
        if (current->exit_code == 0x1337FB) 
        { 
            __asm__ __volatile__("fxrstor64 (%0)" :: "r"(addr) : "memory");
        } 
        else 
        {
            // Primeira vez a usar operações matemáticas: fornece um ambiente limpo de fábrica
            __asm__ __volatile__("fninit");
            
            // Registra a assinatura inicial na estrutura limpa
            __asm__ __volatile__("fxsave64 (%0)" :: "r"(addr) : "memory");
            current->exit_code = 0x1337FB; 
        }
        
        // Registra a nova thread proprietária no bloco Per-CPU do Core
        cpu->fpu_owner_thread = current;
    }
}


/**
 * @brief Tenta expandir a pilha do utilizador caso a falha tenha sido um Stack Overflow controlado.
 * @return 1 se a pilha foi expandida com sucesso, 0 se for um Page Fault real/inválido.
 */
int handle_user_stack_growth(uint64_t fault_address) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    thread_t* current = cpu->current_thread;

    // Se não houver processo ou a falha ocorreu em Ring 0, não é crescimento de pilha
    if (!current || !current->owner) return 0;
    
    process_t* proc = current->owner;

    /*
     * REGRAS DE VALIDAÇÃO DO CRESCIMENTO:
     * 1. O endereço de falha deve estar abaixo do limite atual da stack.
     * 2. O endereço deve estar acima do teto máximo de segurança (ex: stack máxima de 8 MB)
     *    para evitar que o utilizador invada a região do Heap ou código.
     */
    uint64_t max_stack_limit = USER_STACK_VIRTUAL_TOP - USER_STACK_MAX_LIMIT_SIZE;
    
    if (fault_address < proc->stack_limit && fault_address >= max_stack_limit) 
    {
        // Alinha o endereço da falha para o início da página de 4KB (Borda inferior)
        uint64_t page_virt_needed = fault_address & ~0xFFFUL;

        // Calcula quantas páginas precisamos de alocar desde o limite antigo até à necessidade atual
        // Geralmente, alocar uma página de cada vez à medida que falha é o mais performativo
        unsigned long phys_page = pmm_alloc_page();
        if (!phys_page) 
        {
            kprintf("[Stack Growth] Erro: Sem memoria fisica para expandir a pilha do PID %u.\n", proc->pid);
            return 0;
        }

        // Limpa a página por segurança para o utilizador não ler lixo antigo da RAM
        void* scratch = vmm_scratch_map(phys_page);
        memset(scratch, 0, PAGE_SIZE);

        // Mapeia a nova página na PML4 isolada do processo com permissões de utilizador (PAGE_USER_FLAGS)
        PML4_TABLE* target_pml4 = (PML4_TABLE*)vmm_scratch_map(proc->cr3);
        vmm_map_page(target_pml4, page_virt_needed, phys_page, PAGE_USER_FLAGS);

        // Atualiza dinamicamente o novo limite inferior de segurança da pilha no PCB
        proc->stack_limit = page_virt_needed;

        // Invalida a TLB para este endereço específico para o CPU ler o novo mapeamento imediatamente
        __asm__ __volatile__("invlpg (%0)" :: "r"(page_virt_needed) : "memory");

        //kprintf("[Stack Growth] Pilha do PID %u expandida com sucesso ate %p\n", proc->pid, (void*)proc->stack_limit);
        return 1; // Sucesso! O Kernel vai repetir a instrução do utilizador.
    }

    return 0; // Não era um acesso à pilha, deixa o Page Fault tradicional explodir (Crash/SegFault)
}