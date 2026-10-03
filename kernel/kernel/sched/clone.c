/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: clone.c
 *    Description: Subrotina interna de controlo para clonagem de tarefas.
 *                 Implementa a lógica base de duplicação ou partilha de
 *                 contextos lógicos, espaços virtuais (CR3) e recursos (VFS).
 *                 Atua como o motor central para o suporte do fork() clássico
 *                 e criação de fluxos concorrentes baseados em flags.
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 26/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/sched/thread.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/kvmm.h>
#include <kernel/klib.h>
#include <kernel/kernel/sched/clone.h>

/* Referência ao PID incremental externo do process.c */
extern pid_t g_next_pid;

/**
 * sys_clone - Cria um novo fluxo de execução (Thread ou Processo Filho).
 * @frame: Ponteiro para o Stack Frame contendo os registadores salvos da CPU.
 *         frame->rsi conterá as flags binárias de configuração.
 * 
 * Retorna o PID/TID do filho para o chamador, e 0 para o novo fluxo criado.
 */
pid_t clone(stack_frame_t* frame)
{
    uint32_t flags = (uint32_t)frame->rsi; // Lê as flags passadas pelo utilizador em RSI

    /* 1. Identifica o CPU atual e o contexto do Pai */
    cpu_data_block_t* current_cpu = get_current_cpu();
    thread_t* parent_thread = current_cpu->current_thread;
    
    if (!parent_thread || !parent_thread->owner) 
    {
        kprintf("[CLONE ERROR] Contexto pai nulo.\n");
        return -1;
    }
    
    process_t* parent_proc = parent_thread->owner;
    process_t* child_proc  = NULL;

    /* 2. Decisão Arquitetural: É uma Nova Thread ou um Novo Processo? */
    if (flags & CLONE_VM)
    {
        /* CASO A: Criação de Thread (Partilha o mesmo processo) */
        child_proc = parent_proc;
    }
    else
    {
        /* CASO B: Criação de Processo Isolado (Comportamento Fork) */
        child_proc = (process_t*)kmalloc(sizeof(process_t));
        if (!child_proc) 
        {
            kprintf("[CLONE ERROR] Falha ao alocar PCB.\n");
            return -1;
        }
        memset(child_proc, 0, sizeof(process_t));

        /* Clona metadados e o espaço de tabelas de páginas PML4 */
        child_proc->pid         = g_next_pid++;
        child_proc->ppid        = parent_proc->pid; 
        child_proc->state       = PROCESS_EMBRYO;
        child_proc->code_base   = parent_proc->code_base;
        child_proc->heap_start  = parent_proc->heap_start;
        child_proc->heap_end    = parent_proc->heap_end;
        child_proc->stack_top   = parent_proc->stack_top;
        child_proc->stack_limit = parent_proc->stack_limit;

        strncpy(child_proc->pwd,  parent_proc->pwd, MAX_PATH_LENGTH);

        child_proc->cr3 = vmm_clone_address_space(parent_proc->cr3);
        if (child_proc->cr3 == 0) 
        {
            kprintf("[CLONE ERROR] Falha ao clonar CR3.\n");
            kfree(child_proc);
            return -1;
        }

        /* Clona ou limpa a tabela de ficheiros */
        if (flags & CLONE_FILES) {
            for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
                if (parent_proc->file_descriptor_table[i] != NULL) {
                    child_proc->file_descriptor_table[i] = parent_proc->file_descriptor_table[i];
                    child_proc->file_descriptor_table[i]->ref_count++;
                }
            }
        }
    }

    /* 3. Instancia a nova Thread no CPU */
    thread_t* child_thread = user_thread_create(
        (void(*)(void*))frame->rip, 
        NULL, 
        (void*)frame->rsp, 
        parent_thread->cpu_id
    );

    
    if (!child_thread) 
    {
        kprintf("[CLONE ERROR] Falha ao instanciar TCB.\n");
        if (!(flags & CLONE_VM)) kfree(child_proc);
        return -1;
    }

    /* 4. Bloqueio preventivo contra condições de corrida do Scheduler */
    child_thread->state = THREAD_BLOCKED;

    /* 5. Duplica o Stack Frame (Registadores) */
    stack_frame_t* child_frame = (stack_frame_t*)child_thread->context_frame;
    memcpy(child_frame, frame, sizeof(stack_frame_t));

    /* 6. Vinculação Estrutural */
    child_thread->owner = child_proc;
    if (!(flags & CLONE_VM)) {
        child_proc->main_thread = child_thread;
    }

    /* 7. Retorno diferenciado para o fluxo filho */
    child_frame->rax = 0; 

    /* 8. Ativação Atómica */
    if (!(flags & CLONE_VM)) {
        child_proc->state = PROCESS_READY;
    }
    child_thread->state = THREAD_READY;

    process_list_insert(child_proc);

    enqueue_thread(current_cpu, child_thread);
    
    /* 9. Retorno para o Pai (Retorna o PID do novo processo ou o TID da nova thread) */
    return (flags & CLONE_VM) ? child_thread->tid : child_proc->pid;
}