/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: process.c
 *    Description: Implementação do ciclo de vida e alocação de Processos (PCB).
 *                 Gere a criação de novos espaços de endereçamento isolados,
 *                 atribuição de PIDs, libertação em árvore e vínculos com Threads.
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

#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/sched/scheduler.h>

/* 
 * Evita conflitos de dependências cíclicas com cpu.h garantindo 
 * que as estruturas de agendamento e CPU se reconhecem mutuamente.
 */
struct cpu_data_block;
typedef struct cpu_data_block cpu_data_block_t;

#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/arch/x86_64/mm/vmm.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/klib.h>

/* Flags x86_64: Presente (0x1) | Read-Write (0x2) | User-Supervisor (0x4) */
//#define PAGE_USER_FLAGS         (0x1 | 0x2 | 0x4)

/* Flags x86_64: Read-Write (0x2) | User-Supervisor (0x4) */
#define PAGE_USER_FLAGS         (0x2 | 0x4) // Vale 0x6 em vez de 0x7

/* Gerador incremental estático para atribuição única de PIDs */
static pid_t g_next_pid = 1;

/**
 * Aloca um novo processo, isola o espaço de memória (CR3), carrega o binário
 * da aplicação e instancia a thread principal em Ring 3.
 * 
 * @parametro binary_buffer Ponteiro em memória do Kernel onde está o binário bruto.
 * @param binary_size   Tamanho em bytes do binário.
 * @param cpu_id        ID do núcleo onde o processo será inicialmente agendado.
 * @return Ponteiro para a estrutura PCB criada ou NULL em caso de erro.
 */
process_t* process_create(void* binary_buffer, unsigned long binary_size, uint32_t cpu_id)
{
    /* 3. PARSE E CARREGAMENTO DO ELF (O novo bloco futuro)
     * Em vez de fazermos um memcpy cego de 4KB, chamaremos uma função auxiliar:
     * 
     * if (elf_load(proc, elf_buffer, elf_size) != 0) { desfaz_tudo; return NULL; }
     * 
     * Esta função vai ler os cabeçalhos do ELF (Program Headers), alocar as páginas 
     * físicas necessárias para cada seção (.text, .data, .bss) e mapeá-las nos 
     * endereços virtuais que o próprio compilador definiu no binário.
     */

    /* 4. Cria a Thread Principal em Ring 3 
     * O RIP inicial deixará de ser uma macro fixa (USER_CODE_VIRTUAL_BASE) 
     * e passará a usar o Entry Point real lido do cabeçalho ELF:
     * 
     * thread_t* main_th = user_thread_create((void(*)(void))proc->elf_entry, ...);
     */

    /* Validação defensiva do binário */
    if (!binary_buffer || binary_size == 0)
    {
        kprintf("[Process] Erro: Ponteiro ou tamanho do binario invalido.\n");
        return NULL;
    }

    /* 1. Alocação de Memória para o PCB */
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) 
    {
        kprintf("[Process] Erro: Falha ao alocar memoria para o PCB.\n");
        return NULL;
    }

    memset(proc, 0, sizeof(process_t));
    proc->pid = g_next_pid++;
    proc->state = PROCESS_READY;

    /* 2. Configuração da Árvore de Páginas Isolada (PML4) */
    proc->cr3 = vmm_create_address_space();
    if (proc->cr3 == 0)
    {
        kprintf("[Process] Erro: Falha critica ao criar espaco de memoria.\n");
        kfree(proc);
        return NULL;
    }

    /* 
     * 3. MAPEAMENTO E CONFIGURAÇÃO DA MEMÓRIA DO APLICATIVO
     * ------------------------------------------------------------------------
     */
    proc->code_base   = USER_CODE_VIRTUAL_BASE;
    proc->heap_start  = USER_HEAP_VIRTUAL_BASE;
    proc->heap_end    = USER_HEAP_VIRTUAL_BASE; // Tamanho Inicial = 0 Bytes (Dinâmico via sys_brk)
    proc->stack_top   = USER_STACK_VIRTUAL_TOP;
    proc->stack_limit = USER_STACK_VIRTUAL_TOP - USER_STACK_INITIAL_SIZE;

    // A) Aloca a página física para o Código
    unsigned long user_code_phys = pmm_alloc_page();
    if (!user_code_phys)
    {
        kprintf("[Process] Erro: Falha ao alocar pagina fisica para o codigo.\n");
        // pmm_free_page(user_stack_phys);
        pmm_free_page(proc->cr3);
        kfree(proc);
        return NULL;
    }

    /*
     * ============================================================================
     * INJEÇÃO FÍSICA DO BINÁRIO EM MEMÓRIA (CARREGAMENTO)
     * ============================================================================
     * Fazemos a cópia do binário ANTES de mapear as páginas no PML4 do processo.
     * Isto garante que as operações internas do vmm_map_page não colidem
     * com o ponteiro virtual gerado pelo vmm_scratch_map do buffer de código.
     */
    void *scratch_code_ptr = vmm_scratch_map(user_code_phys);
    memset(scratch_code_ptr, 0, PAGE_SIZE);

    unsigned long copy_size = (binary_size > PAGE_SIZE) ? PAGE_SIZE : binary_size;
    memcpy(scratch_code_ptr, binary_buffer, copy_size);

    if (binary_size > PAGE_SIZE)
    {
        kprintf("[Process] Aviso: Binario maior que 4KB.\n");
    }

    /*
     * ============================================================================
     * MAPEAMENTO DAS PÁGINAS NO PML4 DO PROCESSO
     * ============================================================================
     * Agora que o binário já está a salvo no seu frame físico, podemos chamar o
     * vmm_map_page em sequência de forma totalmente segura.
     */
    /*
     * ============================================================================
     * ALOCAÇÃO E MAPEAMENTO EM LOOP DA PILHA DE USUÁRIO (Suporta > 4 KiB)
     * ============================================================================
     * O laço percorre o tamanho total da pilha em blocos de PAGE_SIZE (4 KiB).
     * Mapeia de baixo para cima: do stack_limit até chegar ao stack_top.
     */
    unsigned long num_stack_pages = USER_STACK_INITIAL_SIZE / PAGE_SIZE;

    for (unsigned long i = 0; i < num_stack_pages; i++)
    {
        // Aloca um frame físico para a sub-página atual da pilha
        unsigned long user_stack_phys = pmm_alloc_page();
        if (!user_stack_phys)
        {
            kprintf("[Process] Erro: Falha ao alocar pagina fisica para a sub-pagina %lu da pilha.\n", i);

            /* NOTA DE ROBUSTEZ: Em produção, seria ideal rastrear e desalocar
             * as páginas anteriores ('i' já alocadas) para evitar memory leak. */
            pmm_free_page(proc->cr3);
            kfree(proc);
            return NULL;
        }

        /*
         * Mapeia dinamicamente na árvore isolada do processo (PML4).
         * Endereço virtual avança de 4 KiB em 4 KiB a partir do stack_limit:
         * i = 0 -> proc->stack_limit
         * i = 1 -> proc->stack_limit + 0x1000 (4 KiB)
         */
        vmm_map_page((PML4_TABLE *)vmm_scratch_map(proc->cr3),
                     proc->stack_limit + (i * PAGE_SIZE),
                     user_stack_phys,
                     PAGE_USER_FLAGS);
    }

    /* Mapeia o Código no PML4 do processo */
    vmm_map_page((PML4_TABLE*)vmm_scratch_map(proc->cr3), 
                 proc->code_base, 
                 user_code_phys, 
                 PAGE_USER_FLAGS);

    /* 4. Criação e vinculação da Thread Principal em Ring 3 */
    thread_t* main_th = user_thread_create((void(*)(void))proc->code_base, (void*)proc->stack_top, cpu_id);
    if (!main_th) 
    {
        kprintf("[Process] Erro: Falha ao criar a thread principal.\n");
        pmm_free_page(user_code_phys);
        //pmm_free_page(user_stack_phys);
        pmm_free_page(proc->cr3);
        kfree(proc);
        return NULL;
    }

    main_th->owner = proc;
    proc->main_thread = main_th;

    /* 5. Injeta a tarefa na fila de prontos */
    cpu_data_block_t* cpu = get_cpu_data_block(cpu_id); 
    if (cpu != NULL) 
        enqueue_thread(cpu, main_th);
    else 
        enqueue_thread(get_current_cpu(), main_th);

    kprintf("[Process] Processo %d carregado e isolado! Pilha: 0x%lx | Codigo Virtual: 0x%lx\n", 
            proc->pid, proc->stack_top, proc->code_base);

    return proc;
}

/**
 * Liberta as estruturas alocadas e desvincula os recursos do processo.
 * Limpa recursivamente apenas a metade inferior (User Space) das tabelas físicas.
 */
void process_destroy(process_t* proc)
{
    if (!proc) return;

    kprintf("[Process] A destruir e libertar recursos do processo %d...\n", proc->pid);

    /* 
     * 1. VARREDURA E LIMPEZA DA MMU (ESPAÇO DO UTILIZADOR)
     * Percorre a metade inferior (índices 0 a 255) do PML4 do processo.
     */
    if (proc->cr3 != 0) 
    {
        /* Mapeia o PML4 do processo para inspeção na Janela Temporária */
        PML4_TABLE* pml4 = (PML4_TABLE*)vmm_scratch_map(proc->cr3);
        
        for (int i = 0; i < 256; i++) 
        {
            /* Se a entrada do PML4 estiver presente, aponta para uma PDPT */
            if (pml4[i].p) 
            {
                unsigned long pdpt_phys = (unsigned long)pml4[i].phy_addr_pdpt << 12;
                
                /* Mapeia a PDPT */
                PAGE_DIRECTORY_POINTER_TABLE* pdpt = (PAGE_DIRECTORY_POINTER_TABLE*)vmm_scratch_map(pdpt_phys);
                
                for (int j = 0; j < 512; j++) 
                {
                    /* Se presente e não for uma página gigante (1 GB) */
                    if (pdpt[j].p && !pdpt[j].rs1) 
                    {
                        unsigned long pd_phys = (unsigned long)pdpt[j].phy_addr_pd << 12;
                        
                        /* Mapeia o Diretorio de Páginas (PD) */
                        PAGE_DIRECTORY* pd = (PAGE_DIRECTORY*)vmm_scratch_map(pd_phys);
                        
                        for (int k = 0; k < 512; k++) 
                        {
                            /* Se presente e não for uma página de 2 MB (ps = 0) */
                            if (pd[k].p && !pd[k].ps) 
                            {
                                unsigned long pt_phys = (unsigned long)pd[k].phy_addr_pt << 12;
                                
                                /* Mapeia a Tabela de Páginas (PT) */
                                PAGE_TABLE* pt = (PAGE_TABLE*)vmm_scratch_map(pt_phys);
                                
                                for (int l = 0; l < 512; l++) 
                                {
                                    /* Se a página de dados física de 4KB estiver mapeada, liberta-a */
                                    if (pt[l].p) 
                                    {
                                        unsigned long page_phys = (unsigned long)pt[l].frames << 12;
                                        pmm_free_page(page_phys);
                                    }
                                }
                                
                                /* Liberta a própria PT física após limpar as suas páginas */
                                pmm_free_page(pt_phys);
                                
                                /* Remapeia defensivamente o PD para continuar o laço local de forma estável */
                                pd = (PAGE_DIRECTORY*)vmm_scratch_map(pd_phys);
                            }
                            else if (pd[k].p && pd[k].ps) 
                            {
                                /* Caso tenha mapeado páginas diretas de 2MB, limpa o frame bruto */
                                unsigned long mega_page_phys = (unsigned long)pd[k].phy_addr_pt << 12;
                                pmm_free_page(mega_page_phys);
                            }
                        }
                        
                        /* Liberta o PD físico */
                        pmm_free_page(pd_phys);
                        
                        /* Remapeia a PDPT para continuar o loop local */
                        pdpt = (PAGE_DIRECTORY_POINTER_TABLE*)vmm_scratch_map(pdpt_phys);
                    }
                }
                
                /* Liberta a PDPT física */
                pmm_free_page(pdpt_phys);
                
                /* Remapeia o PML4 para continuar a varredura raiz */
                pml4 = (PML4_TABLE*)vmm_scratch_map(proc->cr3);
            }
        }
        
        /* 2. LIBERTAÇÃO DA RAÍZ DA MMU (O próprio PML4 do processo) */
        pmm_free_page(proc->cr3);
    }

    /* 
     * 3. LIMPEZA DA ESTRUTURA E THREADS ASSOCIADAS
     */
    if (proc->main_thread != NULL) 
    {
        if (proc->main_thread->kernel_stack) 
        {
            /* Recupera a base real do stack alocado (4KB) a partir do topo */
            void* stack_raw = (void*)((uint64_t)proc->main_thread->kernel_stack & ~0xFFFUL);
            kfree(stack_raw);
        }
        kfree(proc->main_thread);
    }

    /* 4. Liberta o Bloco de Controlo do Processo (PCB) */
    kfree(proc);
    
    kprintf("[Process] Processo %d destruido com sucesso.\n", proc->pid);
}
