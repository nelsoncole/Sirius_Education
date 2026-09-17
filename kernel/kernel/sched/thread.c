/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: thread.c
 *    Description: Implementação do ciclo de vida e alocação de Threads.
 *                 Gere a criação de novas tarefas e monta o layout inicial da
 *                 pilha (Stack Frame) compatível com interrupções x86_64.
 * 
 *         Author: Nelson Cole
 *   Created Date: 05/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 05/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/scheduler.h>
#include <kernel/klib.h>

/* ID incremental estático para atribuição única de TIDs */
static uint32_t g_next_tid = 1;

static thread_t* thread_create_common(void (*entry_point)(void), void* user_stack_top, uint32_t cpu_id, uint64_t cs, uint64_t ss)
{
    /* 1. Alocação de Memória */
    thread_t* thread = (thread_t*)kmalloc(sizeof(thread_t));
    void* kernel_stack_raw = (void*)kmalloc(4096); /* Pilha de kernel obrigatória para as ISF */

    if (!thread || !kernel_stack_raw) 
    {
        if (thread) kfree(thread);
        if (kernel_stack_raw) kfree(kernel_stack_raw);
        return NULL;
    }

    /* Limpa a estrutura TCB */
    memset(thread, 0, sizeof(thread_t));

    /* O topo absoluto da pilha limpa (Será usado pelo Syscall e TSS) */
    uint64_t absolute_top = (uint64_t)kernel_stack_raw + 4096;

    /* 2. Moldamos o frame para o Escalonador */
    uint64_t stack_top = absolute_top - sizeof(stack_frame_t);
    stack_frame_t* frame = (stack_frame_t*)stack_top;

    /* Limpa o frame de controlo */
    memset(frame, 0, sizeof(stack_frame_t));

    /* 3. Forja o contexto de privilégios para Ring 3 (User Mode) */
    frame->rip        = (uint64_t)entry_point;
    frame->cs         = cs;   /* Seletor de Código de Utilizador (Ring 3) */
    frame->rflags     = 0x202;  /* Mantém interrupções ativas no espaço do utilizador */
    frame->int_no     = 32;     /* Simula a origem vinda de interrupção externa */
    frame->error_code = 0;
    
    /* Aponta para o topo da pilha virtual dedicada do utilizador */
    frame->rsp        = user_stack_top != NULL ? (uint64_t)user_stack_top : (uint64_t)absolute_top; 
    frame->ss         = ss;   /* Seletor de Dados de Utilizador (Ring 3) */

    /* 4. Preenche as propriedades de controlo do TCB */
    thread->tid          = g_next_tid++;
    thread->state        = THREAD_READY;
    thread->cpu_id       = cpu_id;
   
    // O scheduler continua a ler daqui para restaurar o contexto nas trocas de contexto
    thread->kernel_stack     = (void*)stack_top; 

    // Guarda o topo limpo para o Syscall/TSS redefinirem a pilha do Core
    thread->kernel_stack_top = (void*)absolute_top; 

    thread->next         = NULL;

    /* Envia a thread do utilizador para o processador responsável */
    cpu_data_block_t* target_cpu = get_cpu_data_block(cpu_id);
    enqueue_thread(target_cpu, thread);

    return thread;
}

/**
 * Cria e configura uma nova thread de Kernel, forjando o seu stack inicial.
 */
thread_t* thread_create(void (*entry_point)(void), uint32_t cpu_id)
{
    thread_t* thread = thread_create_common(entry_point, NULL, cpu_id, 0x8, 0x10);
    if (!thread) 
    {
        return NULL;
    }

    return thread;
}

/**
 * Cria e configura uma nova thread de Utilizador (Ring 3), com contexto e isolamento adequados.
 */
thread_t* user_thread_create(void (*entry_point)(void), void* user_stack_top, uint32_t cpu_id)
{
    thread_t* thread = thread_create_common(entry_point, user_stack_top, cpu_id, 0x2B, 0x23);
    if (!thread) 
    {
        return NULL;
    }

    return thread;
}
