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
#include <kernel/kernel/sched/process_loader.h>

/* 
 * Evita conflitos de dependências cíclicas com cpu.h garantindo 
 * que as estruturas de agendamento e CPU se reconhecem mutuamente.
 */
struct cpu_data_block;
typedef struct cpu_data_block cpu_data_block_t;

#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/kvmm.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/klib.h>

/* Flags x86_64: Presente (0x1) | Read-Write (0x2) | User-Supervisor (0x4) */
#define PAGE_USER_FLAGS         (0x1 | 0x2 | 0x4)

/* Gerador incremental estático para atribuição única de PIDs */
static pid_t g_next_pid = 1;

static void process_init_standard_io(process_t* proc) {
    // 1. Limpa toda a tabela para evitar ponteiros lixo
    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
        proc->file_descriptor_table[i] = NULL;
    }

    // 2. Obtém o nó global e unificado da TTY
    vfs_node_t* tty_node = tty_vfs_get_node();

    // 3. Aloca e configura o FD 0 (stdin) - MODO LEITURA
    vfs_file_t* stdin_file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
    stdin_file->node = tty_node;
    stdin_file->offset = 0;
    stdin_file->flags = VFS_MODE_READ;
    proc->file_descriptor_table[0] = stdin_file;

    // 4. Aloca e configura o FD 1 (stdout) - MODO ESCRITA
    vfs_file_t* stdout_file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
    stdout_file->node = tty_node;
    stdout_file->offset = 0;
    stdout_file->flags = VFS_MODE_WRITE;
    proc->file_descriptor_table[1] = stdout_file;

    // 5. Aloca e configura o FD 2 (stderr) - MODO ESCRITA
    vfs_file_t* stderr_file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
    stderr_file->node = tty_node;
    stderr_file->offset = 0;
    stderr_file->flags = VFS_MODE_WRITE;
    proc->file_descriptor_table[2] = stderr_file;
}

/**
 * Aloca um novo processo, isola o espaço de memória (CR3), faz o parse e
 * carregamento dinâmico das seções ELF64, injeta os argumentos na pilha e
 * instancia a thread principal em Ring 3.
 */
process_t* process_create(void* binary_buffer, unsigned long binary_size, int argc, char** argv, uint32_t cpu_id)
{
    /* Validação defensiva do binário e tamanho mínimo do cabeçalho */
    if (!binary_buffer || binary_size < sizeof(Elf64_Ehdr))
    {
        kprintf("[Process] Erro: Ponteiro ou tamanho do binario invalido.\n");
        return NULL;
    }

    // Mapeia o cabeçalho principal ELF64 diretamente em cima do buffer da Pool
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)binary_buffer;

    /* VALIDAÇÃO DE INTEGRIDADE DA ASSINATURA ELF64 */
    if (ehdr->e_ident[0] != ELF_MAGIC_0 || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F')
    {
        kprintf("[Process] Erro Fatal: O buffer nao contem um executavel ELF64 valido.\n");
        return NULL;
    }

    if (ehdr->e_machine != 0x3E) // Mapeia x86_64 Long Mode
    {
        kprintf("[Process] Erro: Executavel nao e compativel com a arquitetura x86_64.\n");
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
    process_init_standard_io(proc);

    /* 2. Configuração da Árvore de Páginas Isolada (PML4) */
    proc->cr3 = vmm_create_address_space();
    if (proc->cr3 == 0)
    {
        kprintf("[Process] Erro: Falha critica ao criar espaco de memoria.\n");
        kfree(proc);
        return NULL;
    }

    /* 
     * 3. CONFIGURAÇÃO DA MEMÓRIA LÓGICA DO APLICATIVO
     * ------------------------------------------------------------------------
     */
    proc->code_base   = ehdr->e_entry; // RIP dinâmico lido do Entry Point real do ELF!
    proc->heap_start  = USER_HEAP_VIRTUAL_BASE;
    proc->heap_end    = USER_HEAP_VIRTUAL_BASE; 
    proc->stack_top   = USER_STACK_VIRTUAL_TOP;
    proc->stack_limit = USER_STACK_VIRTUAL_TOP - USER_STACK_INITIAL_SIZE;

    /*
     * ============================================================================
     * PARSE ELF: MAPEAMENTO E CONFIGURAÇÃO DINÂMICA DE SEGMENTOS PT_LOAD
     * ============================================================================
     */
    Elf64_Phdr* phdr_table = (Elf64_Phdr*)((uint8_t*)binary_buffer + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) 
    {
        Elf64_Phdr* phdr = &phdr_table[i];

        if (phdr->p_type == PT_LOAD) 
        {
            unsigned long page_virt_start = phdr->p_vaddr & ~0xFFFUL;
            unsigned long page_virt_end   = (phdr->p_vaddr + phdr->p_memsz + 0xFFFUL) & ~0xFFFUL;

            for (unsigned long v_addr = page_virt_start; v_addr < page_virt_end; v_addr += PAGE_SIZE) 
            {
                unsigned long segment_phys = pmm_alloc_page();
                if (!segment_phys) 
                {
                    kprintf("[Process] Erro: Falha ao alocar pagina fisica para o segmento ELF.\n");
                    pmm_free_page(proc->cr3);
                    kfree(proc);
                    return NULL;
                }

                void* scratch_ptr = vmm_scratch_map(segment_phys);

                // CASO 1: A página contém dados válidos do ficheiro (Código ou Dados Inicializados)
                if (v_addr + PAGE_SIZE > phdr->p_vaddr && v_addr < phdr->p_vaddr + phdr->p_filesz)
                {
                    unsigned long file_offset = phdr->p_offset;
                    unsigned long page_offset = 0;
                    unsigned long copy_size = PAGE_SIZE;

                    if (v_addr < phdr->p_vaddr) {
                        page_offset = phdr->p_vaddr - v_addr;
                        copy_size -= page_offset;
                        // O início desalinhado da página precisa ser limpo
                        memset(scratch_ptr, 0, page_offset);
                    } else {
                        file_offset += (v_addr - phdr->p_vaddr);
                    }

                    if (v_addr + PAGE_SIZE > phdr->p_vaddr + phdr->p_filesz) {
                        // A página entra no BSS parcial!
                        unsigned long valid_bytes = (phdr->p_vaddr + phdr->p_filesz) - v_addr;
                        copy_size = valid_bytes - page_offset;
                        
                        // LIMPEZA CIRÚRGICA DO BSS PARCIAL: Zera apenas o resto da página
                        unsigned long bss_offset = page_offset + copy_size;
                        memset((uint8_t*)scratch_ptr + bss_offset, 0, PAGE_SIZE - bss_offset);
                    }

                    // Copia rápida do conteúdo real
                    memcpy((uint8_t*)scratch_ptr + page_offset, (uint8_t*)binary_buffer + file_offset, copy_size);
                }
                // CASO 2: A página é PUREZA DE BSS (Variáveis globais não inicializadas)
                else 
                {
                    // Aqui fazemos o memset porque a página inteira vai conter variáveis do programa
                    memset(scratch_ptr, 0, PAGE_SIZE);
                }

                // Obtém o endereço virtual estável da PML4 do novo processo
                PML4_TABLE* target_pml4 = (PML4_TABLE*)vmm_scratch_map(proc->cr3);
                vmm_map_page(target_pml4, v_addr, segment_phys, PAGE_USER_FLAGS);
            }
        }
    }

    /*
     * ============================================================================
     * ALOCAÇÃO E MAPEAMENTO EM LOOP DA PILHA DE USUÁRIO COM INJEÇÃO DE ARGS
     * ============================================================================
     */
    unsigned long num_stack_pages = USER_STACK_INITIAL_SIZE / PAGE_SIZE;
    unsigned long last_stack_phys = 0;

    for (unsigned long i = 0; i < num_stack_pages; i++)
    {
        unsigned long user_stack_phys = pmm_alloc_page();
        if (!user_stack_phys)
        {
            kprintf("[Process] Erro: Falha ao alocar pagina fisica para a sub-pagina %lu da pilha.\n", i);
            pmm_free_page(proc->cr3);
            kfree(proc);
            return NULL;
        }

        // Guarda o frame físico da ÚLTIMA página (onde fica o topo da Stack)
        if (i == num_stack_pages - 1) {
            last_stack_phys = user_stack_phys;
        }

        // Obtém o endereço virtual estável da PML4 do novo processo
        PML4_TABLE* target_pml4 = (PML4_TABLE*)vmm_scratch_map(proc->cr3);
        vmm_map_page(target_pml4, proc->stack_limit + (i * PAGE_SIZE),user_stack_phys,PAGE_USER_FLAGS);
    }

    /*
     * ============================================================================
     * CONSTRUÇÃO ACADÉMICA DA ESTRUTURA ARGC/ARGV DIRETO NA STACK FÍSICA
     * ============================================================================
     * Mapeia o topo físico na scratch window do kernel para injetar os dados.
     */
    if (last_stack_phys != 0) {
        uint8_t* stack_scratch = (uint8_t*)vmm_scratch_map(last_stack_phys);
        
        // Toda a pilha nasce zerada na última página para evitar lixo
        memset(stack_scratch, 0, PAGE_SIZE);

        // O topo real de escrita em memória dentro do buffer de 4KB (anda para trás)
        uint64_t local_offset = PAGE_SIZE; 
        
        // 1. Copia as strings dos argumentos para o fundo da página (ex: "shell\0", "param\0")
        uint64_t* argv_virt_table = (uint64_t*)kmalloc(sizeof(uint64_t) * argc);
        
        for (int i = argc - 1; i >= 0; i--) {
            size_t len = strlen(argv[i]) + 1;
            local_offset -= len;
            memcpy(stack_scratch + local_offset, argv[i], len);
            
            // Calcula o endereço VIRTUAL onde esta string vai morar em Ring 3
            argv_virt_table[i] = proc->stack_top - (PAGE_SIZE - local_offset);
        }

        // Alinhamento estrito a 8 bytes para a tabela de ponteiros
        local_offset &= ~7UL;

        // 2. Escreve a tabela argv contendo os ponteiros virtuais calculados (terminada em NULL)
        local_offset -= sizeof(uint64_t); // Espaço para o ponteiro NULL final
        
        for (int i = argc - 1; i >= 0; i--) {
            local_offset -= sizeof(uint64_t);
            *(uint64_t*)(stack_scratch + local_offset) = argv_virt_table[i];
        }

        // 3. Escreve o valor do ARGC (Número de argumentos)
        local_offset -= sizeof(uint64_t);
        *(uint64_t*)(stack_scratch + local_offset) = (uint64_t)argc;

        // 4. ATUALIZAÇÃO DO PONTEIRO DA PILHA DO PROCESSO
        // O RSP inicial do processo recua para apontar exatamente para o valor do ARGC!
        proc->stack_top = proc->stack_top - (PAGE_SIZE - local_offset);

        kfree(argv_virt_table);
    }

    /* 4. Criação e vinculação da Thread Principal em Ring 3 usando o RSP ajustado */
    thread_t* main_th = user_thread_create((void(*)(void))proc->code_base, (void*)proc->stack_top, cpu_id);
    if (!main_th) 
    {
        kprintf("[Process] Erro: Falha ao criar a thread principal.\n");
        pmm_free_page(proc->cr3);
        kfree(proc);
        return NULL;
    }

    main_th->owner = proc;
    proc->main_thread = main_th;

    /* 5. Injeta a tarefa na fila de prontos do Escalonador */
    cpu_data_block_t* cpu = get_cpu_data_block(cpu_id); 
    if (cpu != NULL) 
        enqueue_thread(cpu, main_th);
    else 
        enqueue_thread(get_current_cpu(), main_th);

    kprintf("[Process] Processo %d [ELF64] pronto com %d argumento(s)! RSP: 0x%lx | RIP: 0x%lx\n", 
            proc->pid, argc, proc->stack_top, proc->code_base);

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
        // LIMPEZA DO VFS: Fecha todos os ficheiros que este processo deixou abertos para evitar memory leaks
        for (int i = 0; i < MAX_FILES_PER_PROCESS; i++)
        {
            if (proc->file_descriptor_table[i] != NULL)
            {
                vfs_close(proc->file_descriptor_table[i]->node);
                kfree(proc->file_descriptor_table[i]);
                proc->file_descriptor_table[i] = NULL;
            }
        }

        kfree(proc->main_thread);
    }

    /* 4. Liberta o Bloco de Controlo do Processo (PCB) */
    kfree(proc);
    
    kprintf("[Process] Processo %d destruido com sucesso.\n", proc->pid);
}

/**
 * get_current_process - Recupera o processo dono da thread ativa no core atual.
 *                       Garante isolamento atómico por hardware em ambiente SMP.
 */
process_t* get_current_process(void) {
    /* 1. Recupera o bloco de controlo da CPU atual via GS/FS */
    cpu_data_block_t *cpu = get_current_cpu();
    if (!cpu || !cpu->current_thread) {
        return NULL;
    }

    /* 2. Extrai o processo dono da thread através da hierarquia do agendador */
    return (process_t*)cpu->current_thread->owner;
}