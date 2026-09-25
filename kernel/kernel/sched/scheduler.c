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

        //kprintf("[GC] Reclamando memória do TID morto: %u\n", current_dead->tid);

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
    cpu->idle_thread->kernel_stack     = (void*)absolute_top; // Inicia vazia no topo

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
 * Encerra voluntariamente o processo atual, liberta o seu espaço de endereçamento 
 * e remove-o permanentemente da fila de execução do Escalonador (Scheduler).
 * 
 * NOTA DE ARQUITETURA: Esta função assume o controlo da Stack e NUNCA mais retorna.
 * 
 * @param code Código de status de finalização que será reportado ao processo pai.
 */
void scheduler_exit(int code)
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

        next = cpu->idle_thread;
        next->state = THREAD_RUNNING;
        cpu->current_thread = next;

        //kprintf("[Kernel] Sem tarefas prontas. Saltando diretamente para a Idle Routine...\n");

        /* 
         * Replicamos a lógica exata de salvaguarda da TSS do seu task_switch,
         * mas apontando para o topo estável inicial da pilha da Idle.
         */
        cpu->tss.rsp0 = (uint64_t)next->kernel_stack_top;
        cpu->kernel_stack_top = (uint64_t)next->kernel_stack_top;
        if (next != cpu->fpu_owner_thread)
        {
            arch_fpu_set_ts();
        }

        /* Consome o CR3 local salvo na inicialização do CPU */
        vmm_switch_pml4(cpu->cr3);

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
            "jmp *%1\n"               // CORRIGIDO: Salto indireto com '*' para o registo
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

    cpu->tss.rsp0 = (uint64_t)next->kernel_stack_top;
    cpu->kernel_stack_top = (uint64_t)next->kernel_stack_top;
    if (next != cpu->fpu_owner_thread) 
    {
        arch_fpu_set_ts(); 
    }

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
        current->kernel_stack = regs;
        
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
        if (current == NULL || current->owner == NULL || current->owner->cr3 != next->owner->cr3) 
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

    return next->kernel_stack;
}

/**
 * scheduler_yield - Permite que uma Thread de Kernel (KThread) abdique voluntariamente
 *                   do processador, devolvendo o controlo ao escalonador de imediato.
 */
void scheduler_yield(void) {
    // 1. Bloqueia as interrupções para garantir que a troca de contexto é atómica
    __asm__ __volatile__("cli");

    cpu_data_block_t* cpu = get_current_cpu();
    thread_t* current = cpu->current_thread;

    // Se estivermos na Idle Task (TID 0) ou sem tarefa válida, não faz sentido ceder
    if (!current || current->tid == 0 || current->state != THREAD_RUNNING) {
        __asm__ __volatile__("sti");
        return;
    }

    /* 
     * MÁGICA DA PREEMPÇÃO VOLUNTÁRIA:
     * Construímos a estrutura registers_t na pilha atual linha por linha,
     * respeitando a ordem exata exigida pelo teu task_switch.
     */
    __asm__ __volatile__ (
        // A. CONTEXTO DE HARDWARE (Salvo ficticiamente em Ring 0)
        "movq %%ss, %%rax\n"
        "pushq %%rax\n"             // registers_t.ss
        "pushq %%rsp\n"             // registers_t.rsp (Pilha atual de kernel)
        "pushfq\n"                  // registers_t.rflags
        "movq %%cs, %%rax\n"
        "pushq %%rax\n"             // registers_t.cs
        "leaq 1f(%%rip), %%rax\n"   // Endereço físico de retorno seguro (etiqueta 1)
        "pushq %%rax\n"             // registers_t.rip
        
        // B. METADADOS DAS MACROS
        "pushq $0\n"                // registers_t.error_code (Nulo falso)
        "pushq $0x81\n"             // registers_t.int_no (Vetor arbitrário para yield)

        // C. REGISTADORES GERAIS (Ordem inversa da tua estrutura registers_t)
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

        // D. INVOCAR O ESCALONADOR
        "movq %%rsp, %%rdi\n"        // Passa o RSP (ponteiro registers_t) como 1º argumento para task_switch
        "call task_switch\n"        // Executa a escolha da próxima tarefa próspera
        
        // E. RESTAURAR A NOVA TAREFA SELECIONADA
        "movq %%rax, %%rsp\n"        // Altera o RSP do CPU para a pilha da nova tarefa (next->kernel_stack)
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
        
        "addq $16, %%rsp\n"         // Limpa registers_t.int_no e error_code da nova pilha
        "iretq\n"                   // Executa o retorno atómico de hardware, restaurando rip, cs e rflags
        
        "1:\n"                      // Ponto de aterragem exato quando esta Thread voltar a acordar!
        :
        :
        : "rax", "rdi", "memory"
    );
}