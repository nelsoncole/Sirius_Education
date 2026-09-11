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
 *  Modified Date: 06/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/scheduler.h>
#include <kernel/arch/x86_64/mm/vmm.h>
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
        cpu->ready_queue_head = thread;
        cpu->ready_queue_tail = thread;
    } 
    else 
    {
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
        return NULL;
    }

    thread_t* thread = cpu->ready_queue_head;
    cpu->ready_queue_head = cpu->ready_queue_head->next;

    if (cpu->ready_queue_head == NULL) 
    {
        cpu->ready_queue_tail = NULL;
    }

    thread->next = NULL;
    return thread;
}

/**
 * Insere uma thread terminada (THREAD_DEAD) na fila de descarte local do CPU.
 */
void enqueue_dead_thread(cpu_data_block_t* cpu, thread_t* thread)
{
    thread->next = NULL;

    if (cpu->dead_queue_tail == NULL) 
    {
        cpu->dead_queue_head = thread;
        cpu->dead_queue_tail = thread;
    } 
    else 
    {
        cpu->dead_queue_tail->next = thread;
        cpu->dead_queue_tail = thread;
    }
}

/**
 * Varre a lista de threads mortas do CPU atual e desaloca a memória física de cada uma.
 */
void scheduler_reclaim_dead_threads(void)
{
    cpu_data_block_t* cpu = get_current_cpu();

    if (cpu->dead_queue_head == NULL) return;

    /* Proteção atómica simples para extrair a lista de descarte */
    __asm__ __volatile__("cli");
    thread_t* current_dead = cpu->dead_queue_head;
    cpu->dead_queue_head = NULL;
    cpu->dead_queue_tail = NULL;
    __asm__ __volatile__("sti");

    while (current_dead != NULL)
    {
        thread_t* next_dead = current_dead->next;

        kprintf("[GC] Reclamando memória do TID morto: %u\n", current_dead->tid);

        /* 
         * TODO: Chamar o desalocador real do seu Kernel
         * kfree(current_dead->kernel_stack_base);
         * kfree(current_dead);
         */

        current_dead = next_dead;
    }
}

/*
 * ----------------------------------------------------------------------------
 * Funções Exportadas do Agendador
 * ----------------------------------------------------------------------------
 */

/**
 * Rotina da Thread Ociosa (Idle Thread).
 */
void idle_thread_routine(void) 
{
    while (1) 
    {
        /* Limpa a memória das threads mortas acumuladas neste núcleo */
        scheduler_reclaim_dead_threads();

        /* Coloca o núcleo do CPU em suspensão segura até à próxima interrupção */
        __asm__ __volatile__("hlt");
    }
}

/**
 * Inicializa o agendador local para o núcleo de processamento atual.
 */
void scheduler_init(void)
{
    cpu_data_block_t* cpu = get_current_cpu();

    cpu->ready_queue_head = NULL;
    cpu->ready_queue_tail = NULL;
    cpu->dead_queue_head  = NULL;
    cpu->dead_queue_tail  = NULL;
    cpu->current_thread   = NULL;

    cpu->idle_thread.tid          = 0;
    cpu->idle_thread.state        = THREAD_READY;
    cpu->idle_thread.cpu_id       = cpu->cpu_id;
    cpu->idle_thread.next         = NULL;
    cpu->idle_thread.kernel_stack = NULL;

    cpu->current_thread = &cpu->idle_thread;
    cpu->current_thread->state = THREAD_RUNNING;
}

/**
 * Interrupção de saída voluntária de um programa (System Call Exit).
 */
uint64_t sys_exit(int code)
{
    __asm__ __volatile__("cli");

    cpu_data_block_t *cpu = get_current_cpu();
    thread_t *current = cpu->current_thread;

    //kprintf("\n[SCI] sys_exit: Aplicativo (TID: %u) encerrou com status %d.\n", 
    //        current ? current->tid : 0, code);

    if (cpu && current)
    {
        current->state = THREAD_DEAD;
        current->exit_code = code;

        /* Move a tarefa atual para a fila de descarte assíncrono */
        enqueue_dead_thread(cpu, current);
    }

    //kprintf("[Kernel] Escolhendo proxima tarefa de forma voluntaria...\n");

    thread_t *next = NULL;

    /* 
     * CICLO DE EXPURGO CORRIGIDO:
     * Remove referências mortas da cabeça da fila, mas para assim
     * que encontra a primeira thread legítima (evita esvaziar a fila).
     */
    while (1) 
    {
        next = dequeue_thread(cpu);
        
        if (next == NULL) 
        {
            break; /* Fila verdadeiramente vazia */
        }

        if (next == current || next->state == THREAD_DEAD) 
        {
            //kprintf("[Kernel Warning] Ignorando referencia fantasma do TID %u na ready_queue.\n", next->tid);
            continue; 
        }

        break; /* Encontrou uma thread válida! */
    }
    
        /* 2. SE A FILA FICOU VAZIA: Cortamos o fluxo e saltamos direto para a Idle */
    if (next == NULL)
    {
        next = &cpu->idle_thread;
        next->state = THREAD_RUNNING;
        cpu->current_thread = next;

        //kprintf("[Kernel] Sem tarefas prontas. Saltando diretamente para a Idle Routine...\n");

        /* 
         * Replicamos a lógica exata de salvaguarda da TSS do seu task_switch,
         * mas apontando para o topo estável inicial da pilha da Idle.
         */
        cpu->tss.rsp0 = (uint64_t)next->kernel_stack + sizeof(stack_frame_t);

        /*
         * COMO VIEMOS DE UMA SYSCALL: 
         * O 'swapgs' já colocou o CPU no espaço do Kernel.
         * Não usamos o 'interrupt_exit_stub' porque ele tentaria fazer um 'iretq' 
         * ou devolver os privilégios para Ring 3, o que gera o #GP.
         *
         * Mudamos o RSP para a pilha da Idle, ligamos as interrupções (sti) 
         * e saltamos nativamente para a rotina.
         */
        __asm__ __volatile__(
            "mov %0, %%rsp\n"         // Altera para a pilha segura da Idle Task
            "sti\n"                   // Reativa o Timer para permitir preempção futura
            "jmp %1\n"                // Salta direto para o loop infinito de 'hlt'
            :
            : "r"(cpu->tss.rsp0), "r"(idle_thread_routine)
            : "memory"
        );

        while (1); 
    }

    /* 3. FLUXO PADRÃO (Apenas se existir OUTRA aplicação REAL de Ring 3 na fila) */
    next->state = THREAD_RUNNING;
    cpu->current_thread = next;

    if (next->owner != NULL && next->owner->cr3 != 0) 
    {
        if (current == NULL || current->owner == NULL || current->owner->cr3 != next->owner->cr3) 
        {
            vmm_switch_pml4(next->owner->cr3);
        }
    }

    cpu->tss.rsp0 = (uint64_t)next->kernel_stack + sizeof(stack_frame_t);

    //kprintf("[Kernel] Alternando para a proxima tarefa REAL (TID: %u)...\n", next->tid);

    /* 
     * Como a próxima tarefa é REAL (Ring 3), ela foi pausada pelo Timer anteriormente.
     * O 'interrupt_exit_stub' vai fazer o 'iretq' legítimo para restaurar o Ring 3 dela.
     */
    __asm__ __volatile__(
        "mov %0, %%rsp\n"
        "jmp interrupt_exit_stub\n"
        :
        : "r"(next->kernel_stack)
        : "memory"
    );

    while (1);
    return 0;
}

/**
 * Realiza a troca de contexto local do núcleo por preempção (Task Switch).
 */
void* task_switch(void* regs) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    thread_t* current = cpu->current_thread;

    if (current != NULL) 
    {
        current->kernel_stack = regs;
        
        if (current->state == THREAD_RUNNING && current->tid != 0) 
        {
            current->state = THREAD_READY;
            enqueue_thread(cpu, current);
        }
    }

    thread_t* next = dequeue_thread(cpu);

    if (next == NULL) 
    {
        next = &cpu->idle_thread;
    }

    next->state = THREAD_RUNNING;
    cpu->current_thread = next;

    if (next->owner != NULL && next->owner->cr3 != 0) 
    {
        if (current == NULL || current->owner == NULL || current->owner->cr3 != next->owner->cr3) 
        {
            vmm_switch_pml4(next->owner->cr3);
        }
    }

    cpu->tss.rsp0 = (uint64_t)next->kernel_stack + sizeof(stack_frame_t);

    return next->kernel_stack;
}
