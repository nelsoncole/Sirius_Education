/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: brk.c
 *    Description: Funções para a sys_brk de Ring3 (Gestão do Heap do Processo).
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
#include <kernel/kernel/sched/process.h>  // Garante acesso às geometrias base do Heap/Stack
#include <kernel/kvmm.h>
#include <kernel/kernel/mm/pmm.h>

/* Flags x86_64: Presente (0x1) | Read-Write (0x2) | User-Supervisor (0x4) */
#define PAGE_USER_FLAGS         (0x1 | 0x2 | 0x4)

/**
 * @brief Altera o limite superior do Heap do processo (Mecanismo brk).
 * @param new_break O novo endereço virtual desejado para o topo do Heap.
 * @return Retorna o break atual em execução (ou o novo em caso de sucesso).
 */
uint64_t brk(uint64_t new_break) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    thread_t* current = cpu->current_thread;

    if (!current || !current->owner) return 0;

    process_t* proc = current->owner;
    uint64_t old_break = (uint64_t)proc->heap_end;

    /* CASO A: Pedido de Consulta Canónica do POSIX */
    if (new_break == 0 || new_break == old_break) 
    {
        return old_break;
    }

    /* VALIDAÇÃO DE SEGURANÇA: Impede invasão ou colisão com a região da Stack */
    if (new_break < USER_HEAP_VIRTUAL_BASE || new_break >= (uint64_t)proc->stack_limit) 
    {
        return old_break;
    }

    /* CASO B: Expandir o Heap */
    if (new_break > old_break) 
    {
        uint64_t page_start = (old_break + PAGE_SIZE - 1) & ~0xFFFUL;
        uint64_t page_end   = (new_break + PAGE_SIZE - 1) & ~0xFFFUL;

        for (uint64_t v_addr = page_start; v_addr < page_end; v_addr += PAGE_SIZE) 
        {
            unsigned long phys_page = pmm_alloc_page();
            if (!phys_page) 
            {
                kprintf("[Sys_Brk] Erro: Exaustao de RAM fisica para o PID %u.\n", proc->pid);
                return old_break;
            }

            void* scratch = vmm_scratch_map(phys_page);
            memset(scratch, 0, PAGE_SIZE);

            PML4_TABLE* target_pml4 = (PML4_TABLE*)vmm_scratch_map(proc->cr3);
            vmm_map_page(target_pml4, v_addr, phys_page, PAGE_USER_FLAGS);
        }
    }
    /* CASO C: Contrair o Heap */
    else if (new_break < old_break) 
    {
        uint64_t page_start = (new_break + PAGE_SIZE - 1) & ~0xFFFUL;
        uint64_t page_end   = (old_break + PAGE_SIZE - 1) & ~0xFFFUL;

        for (uint64_t v_addr = page_start; v_addr < page_end; v_addr += PAGE_SIZE) 
        {
            // Opcional: Implementar desmapeamento futuro se necessário
            // vmm_unmap_page(proc->cr3, v_addr);
        }
    }

    proc->heap_end = (uint64_t)new_break;
    return new_break; 
}