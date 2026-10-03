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

        /* Liberar os recursos da thread morta */
        /*if (current_dead->kernel_stack) {
            kfree(current_dead->kernel_stack);
        }
        kfree(current_dead);*/

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
    cpu->fpu_owner_thread = NULL;

    // 1. Aloca o bloco TCB da Idle Thread
    cpu->idle_thread = (thread_t*)kmalloc(sizeof(thread_t));
    // 2. Aloca a pilha de kernel dedicada para a rotina da Idle Task
    void* idle_stack_raw = (void*)kmalloc(4096);
    
    if (!cpu->idle_thread || !idle_stack_raw) {
        kprintf("[Scheduler] ERRO FATAL: Falha ao alocar recursos para o Idle!\n");
        while(1);
    }
    
    memset(cpu->idle_thread, 0, sizeof(thread_t));
    memset(idle_stack_raw, 0, 4096);

    // 3. Define os limites da pilha dedicada da Idle Task
    uint64_t absolute_top = (uint64_t)idle_stack_raw + 4096;
    cpu->idle_thread->kernel_stack_top = (void*)absolute_top;
    cpu->idle_thread->context_frame     = (void*)absolute_top; // Inicia vazia no topo

    // 4. Configura as propriedades do TCB
    cpu->idle_thread->tid     = 0;
    cpu->idle_thread->state   = THREAD_RUNNING;
    cpu->idle_thread->cpu_id  = cpu->cpu_id;
    cpu->idle_thread->next    = NULL;
    cpu->idle_thread->owner   = NULL; // A Idle pertence ao espaço de Kernel puro

    // O Core arranca a executar diretamente esta Idle Task
    cpu->current_thread = cpu->idle_thread;
}

/**
 * scheduler_ready_process - Ativa o processo e insere a sua thread principal
 *                          na fila de execução da CPU correta de forma segura.
 * @proc: O ponteiro para o PCB do processo que acabou de ser configurado.
 */
void scheduler_ready_process(struct process* proc) 
{
    if (!proc || !proc->main_thread) return;

    // 1. O processo deixa de ser um embrião e passa a estar formalmente ativo
    proc->state = PROCESS_READY;

    // 2. Extrai a thread principal que foi instanciada no processo
    thread_t* main_th = proc->main_thread;

    // 3. Captura o ID da CPU onde a thread foi vinculada no momento da criação
    // (Assumindo que guardas o cpu_id na estrutura da thread ou usas o bloco atual)
    uint32_t cpu_id = main_th->cpu_id; 

    // 4. Injeta com segurança a thread na fila do Core correto (Muda o passo 5 antigo para aqui!)
    cpu_data_block_t* cpu = get_cpu_data_block(cpu_id); 
    if (cpu != NULL) {
        enqueue_thread(cpu, main_th);
    } else {
        enqueue_thread(get_current_cpu(), main_th);
    }

    kprintf("[Scheduler] Thread principal do PID %d despachada para o Core CPU %d.\n", proc->pid, cpu_id);
}

/**
 * Realiza a troca de contexto local do núcleo por preempção (Task Switch).
 */
void* task_switch(void* regs) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    thread_t* current = cpu->current_thread;

    // A Idle Task (TID 0) NUNCA entra na ready_queue
    if (current != NULL) 
    {
        current->context_frame = regs;
        
        // Apenas threads legítimas de utilizador/kernel (TID > 0) voltam para a fila
        if (current->state == THREAD_RUNNING && current->tid != 0) 
        {
            current->state = THREAD_READY;
            enqueue_thread(cpu, current);
        }
    }

    // Tenta buscar a próxima tarefa de utilizador na fila
    thread_t* next = dequeue_thread(cpu);

    if (next == NULL) 
    {
        // Se a tarefa anterior era um utilizador legítimo e ainda está ativa,
        // ela simplesmente continua a executar. Não há queda para a Idle!
        if (current != NULL && current->tid != 0 && current->state == THREAD_READY)
        {
            next = current;
        }
        // Se o utilizador bloqueou, morreu ou o sistema está mesmo ocioso:
        else
        {
            next = cpu->idle_thread;
            next->state = THREAD_RUNNING;
            cpu->current_thread = next;

            /* Configurações físicas do Core para a Idle Task */
            cpu->tss.rsp0         = (uint64_t)next->kernel_stack_top;
            cpu->kernel_stack_top = (uint64_t)next->kernel_stack_top;
            
            if (next != cpu->fpu_owner_thread) {
                arch_fpu_set_ts();
            }

            /* Força o retorno ao espaço de paginação limpo do Kernel */
            vmm_switch_pml4(cpu->cr3);

            /* Salta de forma destrutiva para o loop 'hlt' em Ring 0 */
            __asm__ __volatile__(
                "mov %0, %%rsp\n"         
                "sti\n"                   
                "jmp *%1\n"               
                :
                : "r"(cpu->tss.rsp0), "r"(idle_thread_routine)
                : "memory"
            );
            while (1); 
        }
    }

    /* Execução da próxima tarefa seleccionada */
    next->state = THREAD_RUNNING;
    cpu->current_thread = next;

    // Chaveamento de espaço de endereçamento (CR3)
    if (next->owner != NULL && next->owner->cr3 != 0) 
    {
        if (current == NULL || current->tid == 0 || current->owner == NULL || current->owner->cr3 != next->owner->cr3) 
        {
            vmm_switch_pml4(next->owner->cr3);
        }
    }

    /* Atualiza os ponteiros de controlo de interrupção e syscall do CPU */
    cpu->tss.rsp0         = (uint64_t)next->kernel_stack_top; 
    cpu->kernel_stack_top = (uint64_t)next->kernel_stack_top; 

    // Lazy FPU Switch: Ativa de forma inteligente e passiva
    if (next != cpu->fpu_owner_thread) 
    {
        arch_fpu_set_ts(); 
    }

    return next->context_frame;
}

/**
 * schedule - Força a preempção e troca imediata de contexto de forma agnóstica.
 *            Suporta chamadas vindas de threads em estado THREAD_BLOCKED (ex: sys_waitpid).
 */
void schedule(void)
{
    __asm__ __volatile__("cli");

    cpu_data_block_t* cpu = get_current_cpu();
    thread_t* current = cpu->current_thread;

    if (!current) {
        __asm__ __volatile__("sti");
        return;
    }

    /* 
     * SALVAMENTO ATÓMICO DE CONTEXTO EM INLINE ASSEMBLY:
     * Monta o registers_t estrutural diretamente na stack de Kernel atual.
     */
    __asm__ __volatile__(
        "movq %%ss, %%rax\n"
        "pushq %%rax\n"      // registers_t.ss
        "pushq %%rsp\n"      // registers_t.rsp
        "addq $8, (%%rsp)\n" // Ajusta o RSP fictício para ignorar os pushes do iretq
        "pushfq\n"           // registers_t.rflags
        "movq %%cs, %%rax\n"
        "pushq %%rax\n"           // registers_t.cs
        "leaq 1f(%%rip), %%rax\n" // RIP de aterragem seguro (etiqueta 1)
        "pushq %%rax\n"           // registers_t.rip

        "pushq $0\n"    // registers_t.error_code
        "pushq $0x82\n" // registers_t.int_no

        "pushq %%rbp\n"
        "pushq %%rdi\n"
        "pushq %%rsi\n"
        "pushq %%rdx\n"
        "pushq %%rcx\n"
        "pushq %%rax\n"
        "pushq %%rbx\n"
        "pushq %%r8\n"
        "pushq %%r9\n"
        "pushq %%r10\n"
        "pushq %%r11\n"
        "pushq %%r12\n"
        "pushq %%r13\n"
        "pushq %%r14\n"
        "pushq %%r15\n"

        "movq %%rsp, %%rdi\n" // 1º Parâmetro da task_switch (RDI = RSP atual)
        "call task_switch\n"

        "movq %%rax, %%rsp\n" // RAX contém o RSP retornado da próxima tarefa
        "popq %%r15\n"
        "popq %%r14\n"
        "popq %%r13\n"
        "popq %%r12\n"
        "popq %%r11\n"
        "popq %%r10\n"
        "popq %%r9\n"
        "popq %%r8\n"
        "popq %%rbx\n"
        "popq %%rax\n"
        "popq %%rcx\n"
        "popq %%rdx\n"
        "popq %%rsi\n"
        "popq %%rdi\n"
        "popq %%rbp\n"
        "addq $16, %%rsp\n" // Limpa int_no e error_code
        "iretq\n"           // Salto supersónico para a nova tarefa
        "1:\n"              // PONTO DE ATERRAGEM CRUCIAL QUANDO ESTA THREAD ACORDAR
        :
        :
        : "rax", "rdi", "memory"
    );
}


/**
 * scheduler_yield - Cede voluntariamente o restante time-slice da thread ativa.
 * Apenas executável se a thread ainda estiver em modo executável.
 */ 
void scheduler_yield(void)
{
    asm volatile("cli");
    cpu_data_block_t *cpu = get_current_cpu();
    thread_t *current = cpu->current_thread;
    if (!current || current->tid == 0 || current->state != THREAD_RUNNING)
    {
        asm volatile("sti");
        return;
    } /* Reaproveita o motor completo de desvio atómico */
    schedule();
}

/**
 * scheduler_yield_execve - Cede o controlo sem salvar o contexto antigo.
 * Prepara a thread para acordar diretamente com os novos registos de Ring 3,
 * saltando para o ponto comum de saída do micronúcleo.
 * 
 * @param new_frame Endereço do frame (stack_frame_t) construído na stack de kernel.
 */
void scheduler_yield_execve(uint64_t new_frame)
{
    __asm__ __volatile__("cli");

    /* 
     * BATOTA ARQUITETURAL CONSCIENTE (Sirius_Education):
     * Ignoramos o passado da thread. Atualizamos o contexto da CPU para o 
     * novo frame Ring 3 e saltamos para o stub que já trata o swapgs e iretq!
     */
    __asm__ __volatile__(
        "movq %0, %%rdi\n"             // RDI = Novo frame estruturado
        "call task_switch\n"           // Seleciona a próxima tarefa (Retorna o RSP em RAX)

        "movq %%rax, %%rsp\n"         // Carrega o RSP dinâmico devolvido da tarefa escolhida
        "jmp interrupt_exit_stub\n"   // Desvia para o teu stub oficial de interrupções
        :
        : "r"(new_frame)
        : "rax", "rdi", "memory"
    );
}


/**
 * scheduler_exit - Encerra o fluxo da thread ativa e passa o processador.
 *                  Responsabilidade exclusiva de E/S e Contexto. Não limpa o PCB.
 * 
 * NOTA DE ARQUITETURA: Esta rotina assume o controlo da Stack e NUNCA mais retorna.
 * @code: Código de status de finalização reportado ao Pai.
 */
void scheduler_exit(int exit_code)
{
    /* Bloqueia interrupções para garantir a atomicidade do expurgo */
    __asm__ __volatile__("cli");

    cpu_data_block_t *cpu = get_current_cpu();
    thread_t *current = cpu->current_thread;
    process_t* current_proc = (current != NULL) ? current->owner : NULL;

    /*
     * CORREÇÃO CRÍTICA: Eliminação da recursão infinita.
     * Se não há processo associado (e.g. Thread de Kernel), ignoramos a lógica 
     * de Zombies do VFS e saltamos direto para o encerramento da Thread física.
     */
    if (current_proc != NULL) 
    {
        /* 2. REGISTO DE ESTADO (Transforma o processo em Zombie para o Pai ler) */
        current_proc->exit_code = exit_code;
        current_proc->state     = PROCESS_ZOMBIE; 

        /* 
         * 3. SINALIZAÇÃO E ACORDAR O PAI:
         * Varre a lista global à procura do Pai legítimo. 
         * Se ele estiver bloqueado no waitpid, devolvemo-lo à vida ativa.
         */
        process_list_spinlock_acquire();
        for (process_t* p = g_process_list_head; p != NULL; p = p->next) 
        {
            if (p->pid == current_proc->ppid) 
            {
                // Encontrou o Pai. Verifica se a sua thread principal está em repouso
                if (p->main_thread != NULL && p->main_thread->state == THREAD_BLOCKED) 
                {
                    kprintf("[SCI] sys_exit: Acordando e reinserindo o Pai PID %d na Runqueue...\n", p->pid);
                    
                    /* A. Altera a flag de controle de fluxo do Pai */
                    p->main_thread->state = THREAD_READY;

                    /* B. Insere fisicamente a thread do Pai de volta na fila de execução do Core */
                    enqueue_thread(cpu, p->main_thread); 
                }
                break;
            }
        }
        process_list_spinlock_release();
    }
    else 
    {
        kprintf("[SCHED] scheduler_exit: Encerrando fluxo puro de Kernel (TID: %d).\n", (current != NULL) ? current->tid : 0);
    }

    // 4. LIMPEZA DA THREAD FÍSICA
    if (cpu && current)
    {
        /* Congela o estado físico da Thread na perspetiva do Core */
        current->state = THREAD_DEAD;
        current->exit_code = exit_code;

        /* 
         * Move a tarefa atual para a fila de descarte assíncrono.
         * A 'idle_thread' irá desalocar o TCB e a Kernel Stack desta thread mais tarde.
         */
        enqueue_dead_thread(cpu, current);
    }

    /* 5. Localiza a próxima tarefa pronta na runqueue deste Core */
    thread_t *next = NULL;
    while (1) 
    {
        next = dequeue_thread(cpu);
        
        if (next == NULL) 
        {
            break; /* Fila local vazia */
        }

        /* Ignora referências que já estejam marcadas como mortas */
        if (next == current || next->state == THREAD_DEAD) 
        {
            continue; 
        }

        break; /* Encontrou uma thread válida! */
    }
    
    /* 6. Se não houver tarefas prontas no núcleo, desvia para a Idle Task do Core */
    if (next == NULL)
    {
        next = cpu->idle_thread;
    }

    /* 7. Efetua a troca de contexto atómica definitiva para a nova tarefa */
    next->state = THREAD_RUNNING;
    cpu->current_thread = next;

    /* Troca o espaço virtual de endereçamento (CR3) se mudarmos de processo */
    if (next->owner != NULL && current != NULL && current->owner != next->owner)
    {
        __asm__ __volatile__("mov %0, %%cr3" : : "r"(next->owner->cr3) : "memory");
    }

    /* 
     * 8. SALTO SEM RETORNO DEFINITIVO
     */
    __asm__ __volatile__(
        "movq %0, %%rsp\n"          // Altera o RSP para a pilha da nova tarefa
        "popq %%r15\n"
        "popq %%r14\n"
        "popq %%r13\n"
        "popq %%r12\n"
        "popq %%r11\n"
        "popq %%r10\n"
        "popq %%r9\n"
        "popq %%r8\n"
        "popq %%rbx\n"
        "popq %%rax\n"
        "popq %%rcx\n"
        "popq %%rdx\n"
        "popq %%rsi\n"
        "popq %%rdi\n"
        "popq %%rbp\n"
        "addq $16, %%rsp\n"         // Limpa int_no e error_code
        "iretq\n"                   // Executa o retorno de hardware para a nova tarefa
        :
        : "r"(next->context_frame)
        : "memory"
    );

    /* Salvaguarda física contra falhas de integridade na MMU */
    while (1) 
    {
        __asm__ __volatile__("hlt");
    }
}