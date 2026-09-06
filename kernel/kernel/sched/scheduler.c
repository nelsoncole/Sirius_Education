/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: scheduler.c
 *    Description: Implementação do agendador de tarefas (Task Scheduler).
 *                 Gere as filas de prontos por núcleo (Per-CPU), trocas de
 *                 contexto locais e a execução da thread ociosa (Idle Thread).
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
#include <kernel/arch/x86_64/mm/vmm.h>

/*
 * Nota: Substitui estes cabeçalhos fictícios pelos cabeçalhos reais do teu sistema
 * onde estão declaradas as funções get_current_cpu_id() e get_cpu_data_block().
 */
#include <kernel/klib.h>


/*
 * ----------------------------------------------------------------------------
 * Funções Internas de Gestão da Fila (Runqueue)
 * ----------------------------------------------------------------------------
 */

/**
 * Adiciona uma thread ao fim da fila de prontos de um CPU específico.
 */
void enqueue_thread(cpu_data_block_t* cpu, thread_t* thread) 
{
    thread->next = NULL;

    if (cpu->ready_queue_tail == NULL) 
    {
        /* Fila vazia: a thread torna-se a cabeça e a cauda */
        cpu->ready_queue_head = thread;
        cpu->ready_queue_tail = thread;
    } 
    else 
    {
        /* Adiciona ao fim da lista ligada */
        cpu->ready_queue_tail->next = thread;
        cpu->ready_queue_tail = thread;
    }
}

/**
 * Remove e retorna a próxima thread do início da fila de prontos de um CPU.
 */
thread_t* dequeue_thread(cpu_data_block_t* cpu) 
{
    if (cpu->ready_queue_head == NULL) 
    {
        return NULL; /* Fila vazia */
    }

    thread_t* thread = cpu->ready_queue_head;
    cpu->ready_queue_head = cpu->ready_queue_head->next;

    /* Se a fila ficou vazia, ajusta a cauda */
    if (cpu->ready_queue_head == NULL) 
    {
        cpu->ready_queue_tail = NULL;
    }

    thread->next = NULL;
    return thread;
}

/*
 * ----------------------------------------------------------------------------
 * Funções Exportadas do Agendador
 * ----------------------------------------------------------------------------
 */

/**
 * Rotina da Thread Ociosa (Idle Thread).
 * Executada continuamente quando não existem outras tarefas prontas.
 */
void idle_thread_routine(void) 
{
    while (1) 
    {
        /* 
         * Coloca o CPU em estado de baixo consumo até à próxima interrupção.
         * Em x86/x64, a instrução 'hlt' suspende o núcleo de forma segura.
         */
        __asm__ __volatile__("hlt");
    }
}

/**
 * Inicializa o agendador local para o núcleo de processamento atual.
 * Deve ser chamada individualmente pelo BSP e por cada AP no arranque.
 */
void scheduler_init(void)
{
    cpu_data_block_t* cpu = get_current_cpu();

    /* 1. Inicializa os ponteiros da fila local de tarefas (Runqueue) */
    cpu->ready_queue_head = NULL;
    cpu->ready_queue_tail = NULL;
    cpu->current_thread   = NULL;

    /* 2. Configura as informações base da Idle Thread local */
    cpu->idle_thread.tid          = 0; /* TID reservada para a Idle Thread */
    cpu->idle_thread.state        = THREAD_READY;
    cpu->idle_thread.cpu_id       = cpu->cpu_id;
    cpu->idle_thread.next         = NULL;
    
    /* 
     * NOTA DE ARQUITETURA:
     * O stack do kernel para a idle_thread pode ser o próprio stack padrão 
     * com que este núcleo iniciou (o stack de boot do BSP ou o alocado para o AP).
     * O ponteiro 'kernel_stack' será atualizado na primeira chamada ao task_switch.
     */
    cpu->idle_thread.kernel_stack = NULL;

    /* Define a idle_thread como a tarefa inicial ativa do CPU até surgir concorrência */
    cpu->current_thread = &cpu->idle_thread;
    cpu->current_thread->state = THREAD_RUNNING;
}

/**
 * Realiza a troca de contexto local do núcleo (Task Switch).
 * Chamada de dentro do handler do Timer do LAPIC (Interrupção 32).
 */
void* task_switch(void* regs) 
{
    /* 1. Identifica o CPU atual e localiza o seu bloco de dados */
    cpu_data_block_t* cpu = get_current_cpu();
    thread_t* current = cpu->current_thread;

    /* 2. Salva o contexto da thread que estava a correr */
    if (current != NULL) 
    {
        /* Guarda o ponteiro do stack frame atual no TCB da thread */
        current->kernel_stack = regs;
        
        /* 
         * CRÍTICO PARA SMP: A Idle Thread (TID 0) NUNCA volta para a fila de prontos.
         * Se ela entrasse na fila, bloquearia o processamento das threads reais.
         */
        if (current->state == THREAD_RUNNING && current->tid != 0) 
        {
            current->state = THREAD_READY;
            enqueue_thread(cpu, current);
        }
    }

    /* 3. Seleciona a próxima thread a ser executada (Algoritmo Round-Robin) */
    thread_t* next = dequeue_thread(cpu);

    if (next == NULL) 
    {
        /* Se não há tarefas prontas, ativa a Idle Thread local deste CPU */
        next = &cpu->idle_thread;
    }

    /* 4. Atualiza o estado para a nova thread */
    next->state = THREAD_RUNNING;
    cpu->current_thread = next;

    /* 
     * ============================================================================
     * CHAVEAMENTO FÍSICO DA MMU (TROCA DE CR3)
     * ============================================================================
     * Se a próxima thread pertencer a um processo isolado, forçamos o processador
     * a carregar o novo diretório de páginas (PML4).
     */
    if (next->owner != NULL && next->owner->cr3 != 0) 
    {
        /* 
         * Para otimização de performance, apenas trocamos o CR3 se o processo 
         * destino for diferente do processo que estava a rodar anteriormente.
         */
        if (current == NULL || current->owner == NULL || current->owner->cr3 != next->owner->cr3) 
        {
            /* Chama a função nativa do seu vmm.h */
            vmm_switch_pml4(next->owner->cr3);
        }
    }
    /* ============================================================================ */

    /* 
     * 5. AJUSTE DA INTERRUPÇÃO EM HARDWARE (TSS RSP0):
     * Quando o CPU estiver em Ring 3 e sofrer uma interrupção, ele precisa de saber
     * onde recomeça o topo virgem da pilha de kernel desta thread específica.
     */
    cpu->tss.rsp0 = (uint64_t)next->kernel_stack + sizeof(stack_frame_t);

    /* 
     * 6. RETORNO DO NOVO CONTEXTO:
     * Retornamos o ponteiro em RAX.
     * O teu interrupt.asm vai ler este retorno e fazer o chaveamento físico seguro.
     */
    return next->kernel_stack;
}