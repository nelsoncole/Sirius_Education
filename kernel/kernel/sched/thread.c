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

/**
 * Cria e configura uma nova thread de Kernel, forjando o seu stack inicial.
 */
thread_t* thread_create(void (*entry_point)(void), uint32_t cpu_id)
{
    /* 1. Alocação de Memória */
    thread_t* thread = (thread_t*)kmalloc(sizeof(thread_t));
    void* stack_raw  = (void*)kmalloc(4096); /* Aloca 4KB de pilha de kernel */

    if (!thread || !stack_raw) 
    {
        if (thread) kfree(thread);
        if (stack_raw) kfree(stack_raw);
        return NULL;
    }

    /* Limpa a estrutura TCB */
    memset(thread, 0, sizeof(thread_t));

    /* 2. Posiciona o ponteiro no topo da pilha alocada (Stacks crescem para baixo) */
    uint64_t stack_top = (uint64_t)stack_raw + 4096;

    /* Reserva espaço físico para moldar o frame inicial de interrupção */
    stack_top -= sizeof(stack_frame_t);
    stack_frame_t* frame = (stack_frame_t*)stack_top;

    /* Limpa o frame para garantir que os registos de uso geral começam a zero */
    memset(frame, 0, sizeof(stack_frame_t));

    /* 3. Forja o contexto que o IRETQ irá desempilhar em Ring 0 */
    frame->rip        = (uint64_t)entry_point;
    frame->cs         = 0x08;   /* Seletor de Código do teu Kernel */
    frame->rflags     = 0x202;  /* Ativa a flag IF (Interrupt Flag) */
    frame->int_no     = 32;     /* Simula o vetor do timer */
    frame->error_code = 0;
    
    /* 
     * Como o teu stub em Assembly faz o cálculo completo da estrutura incluindo os 
     * campos RSP e SS, alimentamos os valores padrões nativos de Kernel para evitar 
     * desvios de desalinhamento na execução dos POPs subsequentes.
     */
    frame->rsp        = stack_top + sizeof(stack_frame_t); 
    frame->ss         = 0x10;   /* Seletor de Dados do teu Kernel */

    /* 4. Preenche as propriedades de controlo do TCB */
    thread->tid          = g_next_tid++;
    thread->state        = THREAD_READY;
    thread->cpu_id       = cpu_id;
    thread->kernel_stack = (void*)stack_top; /* Define onde o task_switch começará a ler */
    thread->next         = NULL;

    /* Obtém o bloco do CPU alvo e insere a thread na fila (runqueue) do núcleo selecionado */
    cpu_data_block_t* target_cpu = get_cpu_data_block(cpu_id);
    enqueue_thread(target_cpu, thread);

    return thread;
}

/**
 * Cria e configura uma nova thread de Utilizador (Ring 3), com contexto e isolamento adequados.
 */
thread_t* user_thread_create(void (*entry_point)(void), void* user_stack_top, uint32_t cpu_id)
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

    /* 2. Moldamos o frame no topo da pilha de KERNEL (Onde o escalonador vai ler o contexto) */
    uint64_t stack_top = (uint64_t)kernel_stack_raw + 4096;
    stack_top -= sizeof(stack_frame_t);
    stack_frame_t* frame = (stack_frame_t*)stack_top;

    /* Limpa o frame de controlo */
    memset(frame, 0, sizeof(stack_frame_t));

    /* 3. Forja o contexto de privilégios para Ring 3 (User Mode) */
    frame->rip        = (uint64_t)entry_point;
    frame->cs         = 0x1B;   /* Seletor de Código de Utilizador (Ring 3) */
    frame->rflags     = 0x202;  /* Mantém interrupções ativas no espaço do utilizador */
    frame->int_no     = 32;     /* Simula a origem vinda de interrupção externa */
    frame->error_code = 0;
    
    /* Aponta para o topo da pilha virtual dedicada do utilizador */
    frame->rsp        = (uint64_t)user_stack_top; 
    frame->ss         = 0x23;   /* Seletor de Dados de Utilizador (Ring 3) */

    /* 4. Preenche as propriedades de controlo do TCB */
    thread->tid          = g_next_tid++;
    thread->state        = THREAD_READY;
    thread->cpu_id       = cpu_id;
    thread->kernel_stack = (void*)stack_top; /* O scheduler lê sempre o registo guardado na pilha de kernel */
    thread->next         = NULL;

    /* Envia a thread do utilizador para o processador responsável */
    cpu_data_block_t* target_cpu = get_cpu_data_block(cpu_id);
    enqueue_thread(target_cpu, thread);

    return thread;
}
