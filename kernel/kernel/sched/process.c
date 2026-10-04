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
 *  Modified Date: 22/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/sched/scheduler.h>
#include <kernel/kernel/sched/process_loader.h>
#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/kernel/mm/pmm.h>
#include <kernel/kvmm.h>
#include <kernel/klib.h>
#include <kernel/kernel/core/spinlock.h>

/* Flags x86_64: Presente (0x1) | Read-Write (0x2) | User-Supervisor (0x4) */
#define PAGE_USER_FLAGS         (0x1 | 0x2 | 0x4)

/* Gerador incremental estático para atribuição única de PIDs */
pid_t g_next_pid = 1;

/* Registo global estável para auditoria e gestão de parentesco */
process_t* g_process_list_head = NULL;
/* Spinlock central para proteção da lista em ambiente SMP */
spinlock_t g_process_list_lock = {0};

void process_list_spinlock_acquire(void) {
    spinlock_acquire(&g_process_list_lock);
}

void process_list_spinlock_release(void) {
    spinlock_release(&g_process_list_lock);
}

/**
 * process_list_insert - Insere atonicamente um novo processo na lista global.
 * @proc: Ponteiro para o Bloco de Controlo do Processo (PCB) a ser inserido.
 */
void process_list_insert(process_t* proc)
{
    if (!proc) return;

    /* Bloqueia o Spinlock: Se outra CPU estiver a mexer na lista, este núcleo espera aqui */
    spinlock_acquire(&g_process_list_lock);

    /* Insere na cabeça da lista ligada */
    proc->next = g_process_list_head;
    g_process_list_head = proc;

    /* Liberta o Spinlock para as restantes CPUs poderem aceder */
    spinlock_release(&g_process_list_lock);
}

/**
 * process_list_remove - Remove atonicamente um processo da lista global.
 * @proc: Ponteiro para o PCB a ser removido.
 */
void process_list_remove(process_t* proc)
{
    if (!proc || !g_process_list_head) return;

    /* Garante exclusão mútua em ambiente SMP */
    spinlock_acquire(&g_process_list_lock);

    /* Caso especial: O processo a remover é o primeiro da lista */
    if (g_process_list_head == proc) 
    {
        g_process_list_head = g_process_list_head->next;
        spinlock_release(&g_process_list_lock);
        return;
    }

    /* Varrer a lista para encontrar o nó anterior */
    process_t* current = g_process_list_head;
    while (current->next != NULL && current->next != proc) 
    {
        current = current->next;
    }

    /* Se encontrou o nó, desvincula-o da corrente */
    if (current->next == proc) 
    {
        current->next = proc->next;
    }

    /* Liberta a região crítica */
    spinlock_release(&g_process_list_lock);
}

/**
 * process_init_standard_io - Inicializa os canais padrão (0, 1, 2) de um processo.
 * @proc:    O processo que está a ser configurado.
 * @io_path: Caminho literal do terminal alvo (ex: "/dev/tty1", "/dev/pts/0"). 
 *           Se NULL, faz fallback para a consola física "/dev/tty0".
 */
void process_init_standard_io(process_t* proc, const char* io_path) {
    if (!proc) return;

    // 1. Limpa toda a tabela para evitar ponteiros lixo residuais
    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
        proc->file_descriptor_table[i] = NULL;
    }

    // 2. Determina dinamicamente o terminal alvo (Usa /dev/tty0 físico se io_path for NULL)
    const char* target_path = (io_path != NULL) ? io_path : "/dev/tty0";

    // Obtém o nó do terminal específico (Pode ser /dev/ttyX ou /dev/pts/X)
    vfs_node_t* term_node = vfs_path_to_node(target_path);
    if (!term_node) {
        kprintf("[PROC ERROR] Falha grave: %s nao encontrado para E/S padrao.\n", target_path);
        return;
    }

    // Executa a abertura polimórfica para instanciar o driver privado correto (TTY ou PTY)
    if (term_node->ops && term_node->ops->open) {
        term_node->ops->open(term_node, 0x0002); // Abre em modo O_RDWR (Gera tfs_pty_replica_open se for pty)
    }

    // 3. Aloca a descrição intermédia APENAS para o FD 0 (stdin)
    vfs_file_t* stdin_file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
    if (!stdin_file) {
        kprintf("[PROC ERROR] Falha de alocacao de memoria para stdin.\n");
        return;
    }
    memset(stdin_file, 0, sizeof(vfs_file_t));
    
    stdin_file->node = term_node; // Associa dinamicamente o nó resolvido pelo VFS
    stdin_file->offset = 0;
    stdin_file->flags = 0x0002;   // Modo O_RDWR
    stdin_file->ref_count = 1;

    // Guarda fisicamente na Entrada Padrão do processo alvo
    proc->file_descriptor_table[0] = stdin_file;

    // 4. Clona stdin (0) -> Saída Padrão stdout (1)
    k_dup2(proc, 0, 1);

    // 5. Clona stdin (0) -> Saída de Erro stderr (2)
    k_dup2(proc, 0, 2);

    kprintf("[PROC] Canais de E/S padrao (0, 1, 2) linkados ao PID %d via %s.\n", proc->pid, target_path);
}

/**
 * Faz o parsing estrutural do cabeçalho ELF, mapeia os segmentos PT_LOAD na tabela 
 * de páginas do processo atual e reconstrói a stack do utilizador com os argumentos.
 * 
 * @param proc           Ponteiro para o processo atual (PCB).
 * @param binary_buffer  Ponteiro para o buffer alinhado contendo o ficheiro ELF.
 * @param binary_size    Tamanho total do binário em bytes.
 * @param argc           Contagem de argumentos.
 * @param argv           Array de strings dos argumentos.
 * @return Retorna o Entry Point (e_entry) em caso de sucesso, ou 0 em caso de erro.
 */
uintptr_t elf_parse_and_map(process_t* proc, void* binary_buffer, unsigned long binary_size, int argc, char** argv)
{
    /* Validação defensiva do binário e tamanho mínimo do cabeçalho */
    if (!proc || !binary_buffer || binary_size < sizeof(Elf64_Ehdr))
    {
        kprintf("[Process] Erro: Parâmetros ou tamanho de binário inválido para o parser ELF.\n");
        return 0;
    }

    // Mapeia o cabeçalho principal ELF64 diretamente em cima do buffer da Pool
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)binary_buffer;

    /* VALIDAÇÃO DE INTEGRIDADE DA ASSINATURA ELF64 */
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' || ehdr->e_ident[3] != 'F')
    {
        kprintf("[Process] Erro Fatal: O buffer não contém um executável ELF64 válido.\n");
        return 0;
    }

    if (ehdr->e_machine != 0x3E) // Mapeia x86_64 Long Mode
    {
        kprintf("[Process] Erro: Executável não é compatível com a arquitetura x86_64.\n");
        return 0;
    }

    /* 
     * 1. RECONFIGURAÇÃO DA MEMÓRIA LÓGICA DO APLICATIVO EXISTENTE
     * ------------------------------------------------------------------------
     */
    proc->code_base   = ehdr->e_entry; // Novo RIP dinâmico lido do Entry Point real do ELF
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
                    kprintf("[Process] Erro: Falha ao alocar página física para o segmento ELF.\n");
                    return 0;
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
                        file_offset += page_offset; // CORREÇÃO: Sincroniza o offset do ficheiro para a leitura parcial
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
                    memset(scratch_ptr, 0, PAGE_SIZE);
                }

                // Obtém o endereço virtual estável da PML4 do processo atual
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
    unsigned long num_stack_pages = (USER_STACK_INITIAL_SIZE / PAGE_SIZE);
    unsigned long last_stack_phys = 0;

    for (unsigned long i = 0; i < num_stack_pages; i++)
    {
        unsigned long user_stack_phys = pmm_alloc_page();
        if (!user_stack_phys)
        {
            kprintf("[Process] Erro: Falha ao alocar página física para a sub-página %lu da pilha.\n", i);
            return 0;
        }

        // Guarda o frame físico da ÚLTIMA página (onde fica o topo da Stack)
        if (i == num_stack_pages - 1) {
            last_stack_phys = user_stack_phys;
        }

        // Obtém o endereço virtual estável da PML4 do processo atual
        PML4_TABLE* target_pml4 = (PML4_TABLE*)vmm_scratch_map(proc->cr3);
        vmm_map_page(target_pml4, proc->stack_limit + (i * PAGE_SIZE), user_stack_phys, PAGE_USER_FLAGS);
    }

    /*
     * ============================================================================
     * CONSTRUÇÃO DA ESTRUTURA ARGC/ARGV DIRETO NA STACK FÍSICA
     * ============================================================================
     */
    if (last_stack_phys != 0) {
        uint8_t* stack_scratch = (uint8_t*)vmm_scratch_map(last_stack_phys);
        memset(stack_scratch, 0, PAGE_SIZE);

        uint64_t local_offset = PAGE_SIZE; 
        
        uint64_t* argv_virt_table = (uint64_t*)kmalloc(sizeof(uint64_t) * argc);
        if (!argv_virt_table) {
            kprintf("[Process] Erro: Falha ao alocar tabela virtual de argumentos.\n");
            return 0;
        }
        
        // 1. Copia as strings dos argumentos para o fundo da página
        for (int i = argc - 1; i >= 0; i--) {
            size_t len = strlen(argv[i]) + 1;
            
            // CORREÇÃO: Barreira defensiva contra estouro de stack na página única de boot
            if (local_offset < len || (local_offset - len) < (sizeof(uint64_t) * (argc + 2))) {
                kprintf("[Process] Erro Fatal: Argumentos excedem o espaço reservado na página de stack.\n");
                kfree(argv_virt_table);
                return 0;
            }

            local_offset -= len;
            memcpy(stack_scratch + local_offset, argv[i], len);
            
            // Calcula o endereço VIRTUAL onde esta string vai morar em Ring 3
            argv_virt_table[i] = proc->stack_top - (PAGE_SIZE - local_offset);
        }

        // Alinhamento estrito a 8 bytes para a tabela de ponteiros
        local_offset &= ~7UL;

        // 2. Escreve a tabela de ponteiros (argv[]) e argc na stack conforme o ABI x86_64
        local_offset -= sizeof(uint64_t) * (argc + 1); // +1 para o terminador NULL
        uint64_t* local_argv = (uint64_t*)(stack_scratch + local_offset);
        
        for (int i = 0; i < argc; i++) {
            local_argv[i] = argv_virt_table[i];
        }
        local_argv[argc] = 0; // argv[argc] = NULL

        kfree(argv_virt_table);

        // Ajusta o offset para empurrar o valor de argc (System V ABI)
        local_offset -= sizeof(uint64_t);
        *(uint64_t*)(stack_scratch + local_offset) = (uint64_t)argc;

        // Atualiza a stack_top lógica do processo para o ponto exato onde o RSP deve iniciar
        proc->stack_top = proc->stack_top - (PAGE_SIZE - local_offset);

    }

    // Retorna o endereço de entrada mapeado com sucesso para atualizar os registos da thread
    return (uintptr_t)ehdr->e_entry;
}

/**
 * Aloca um novo processo, isola o espaço de memória (CR3), delega o parse e
 * carregamento dinâmico das seções ELF64 e a injeção da pilha de utilizador.
 */
process_t* process_create(void* binary_buffer, unsigned long binary_size, int argc, char** argv, uint32_t cpu_id)
{
    /* 1. Validação defensiva inicial (antes de alocar recursos pesados) */
    if (!binary_buffer || binary_size < sizeof(Elf64_Ehdr))
    {
        kprintf("[Process] Erro: Ponteiro ou tamanho do binario invalido.\n");
        return NULL;
    }

    /* 2. Alocação de Memória para o PCB */
    process_t* proc = (process_t*)kmalloc(sizeof(process_t));
    if (!proc) 
    {
        kprintf("[Process] Erro: Falha ao alocar memoria para o PCB.\n");
        return NULL;
    }

    memset(proc, 0, sizeof(process_t));
    proc->pid = g_next_pid++;
    
    // Nasce como um embrião/protegido, impedindo o escalonador de o puxar antes do tempo
    proc->state = PROCESS_EMBRYO;

     /* 
     * INICIALIZAÇÃO OBRIGATÓRIA DO DIRETÓRIO DE TRABALHO (PWD)
     * Garante que o processo começa de forma limpa e segura no diretório raiz.
     */
    strncpy(proc->pwd, "/", MAX_PATH_LENGTH);

    /* 3. Configuração da Árvore de Páginas Isolada (PML4) */
    proc->cr3 = vmm_create_address_space();
    if (proc->cr3 == 0)
    {
        kprintf("[Process] Erro: Falha critica ao criar espaco de memoria.\n");
        kfree(proc);
        return NULL;
    }

    /* 
     * 4. DELEGAÇÃO DO PARSING E MAPEAMENTO ELF
     * Reaproveita a nova função modular eliminando repetição de código (DRY)
     */
    uintptr_t entry_point = elf_parse_and_map(proc, binary_buffer, binary_size, argc, argv);
    if (entry_point == 0)
    {
        kprintf("[Process] Erro: Falha ao processar e mapear a estrutura do binário ELF.\n");
        
        // Como o elf_parse_and_map pode ter alocado algumas páginas parciais antes de falhar,
        // o ideal aqui é chamar a limpeza que criámos para purgar as tabelas.
        process_flush_user_space(proc->cr3);
        
        pmm_free_page(proc->cr3);
        kfree(proc);
        return NULL;
    }

    /* 4. Criação e vinculação da Thread Principal em Ring 3 usando o RSP ajustado */
    thread_t* main_th = user_thread_create(
        (void(*)(void*))proc->code_base,
        NULL,
        (void*)proc->stack_top,
        cpu_id
    );

    if (!main_th) 
    {
        kprintf("[Process] Erro: Falha ao criar a thread principal.\n");
        pmm_free_page(proc->cr3);
        kfree(proc);
        return NULL;
    }

    main_th->owner = proc;
    proc->main_thread = main_th;

    kprintf("[Process] Processo %d [ELF64] pronto com %d argumento(s)! RSP: 0x%lx | RIP: 0x%lx\n", 
            proc->pid, proc->stack_top, proc->code_base);

    process_list_insert(proc);
    return proc;
}

/**
 * Liberta as estruturas alocadas e desvincula os recursos do espaço do utilizador.
 * Limpa recursivamente apenas a metade inferior (User Space) das tabelas físicas
 * associadas ao CR3, preparando o processo atual para o fluxo de sobreposição.
 */
int process_flush_user_space(uint64_t pml4_phys)
{
    if (!pml4_phys) return -1;

    kprintf("[Process] A limpar User Space do processo para sobreposição...\n");
    
    // Limpa apenas as primeiras 256 entradas (User Space)
    for (int i = 0; i < 256; i++) 
    {
        /* GARANTIA JIT: Remapeia o PML4 na Janela 2 a cada ciclo para limpar resíduos de TLB */
        PML4_TABLE* pml4 = (PML4_TABLE*)vmm_scratch_map_internal(pml4_phys, 2);

        if (pml4[i].p) 
        {
            unsigned long pdpt_phys = (unsigned long)pml4[i].phy_addr_pdpt << 12;
            
            for (int j = 0; j < 512; j++) 
            {
                /* BLINDAGEM: Restaura a integridade da Janela 3 antes de ler pdpt[j] */
                PAGE_DIRECTORY_POINTER_TABLE* pdpt = (PAGE_DIRECTORY_POINTER_TABLE*)vmm_scratch_map_internal(pdpt_phys, 3);

                if (pdpt[j].p && !pdpt[j].rs1) 
                {
                    unsigned long pd_phys = (unsigned long)pdpt[j].phy_addr_pd << 12;
                    
                    for (int k = 0; k < 512; k++) 
                    {
                        /* Restaura a integridade da Janela 4 antes de ler pd[k] */
                        PAGE_DIRECTORY* pd = (PAGE_DIRECTORY*)vmm_scratch_map_internal(pd_phys, 4);

                        if (pd[k].p && !pd[k].ps) 
                        {
                            unsigned long pt_phys = (unsigned long)pd[k].phy_addr_pt << 12;
                            
                            /* Mapeia a PT na Janela 5 */
                            PAGE_TABLE* pt = (PAGE_TABLE*)vmm_scratch_map_internal(pt_phys, 5);
                            
                            for (int l = 0; l < 512; l++) 
                            {
                                if (pt[l].p) 
                                {
                                    unsigned long page_phys = (unsigned long)pt[l].frames << 12;
                                    pmm_free_page(page_phys);
                                }
                            }
                            
                            /* Liberta a PT física após expurgar as páginas */
                            pmm_free_page(pt_phys);
                        }
                        else if (pd[k].p && pd[k].ps) 
                        {
                            unsigned long mega_page_phys = (unsigned long)pd[k].phy_addr_pt << 12;
                            pmm_free_page(mega_page_phys);
                        }
                    }
                    
                    /* Liberta o PD físico */
                    pmm_free_page(pd_phys);
                }
            }
            
            /* Liberta a PDPT física */
            pmm_free_page(pdpt_phys);
            
            /* Desvincula a entrada no PML4 de User Space */
            pml4[i].p = 0;
            pml4[i].phy_addr_pdpt = 0;
        }
    }

    /* Atualiza o TLB (Invalida as entradas antigas de User Space) */
    __asm__ __volatile__("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "rax", "memory");
    
    kprintf("[Process] User Space do processo expurgado com sucesso.\n");

    return 0;
}

/**
 * Liberta as estruturas alocadas e desvincula os recursos do processo.
 * Limpa recursivamente apenas a metade inferior (User Space) das tabelas físicas.
 */
void process_destroy(process_t* proc)
{
    if (!proc) return;

    kprintf("[Process] A destruir e libertar recursos do processo %d com janelas fixas...\n", proc->pid);

    if (proc->cr3 != 0) 
    {
        unsigned long pml4_phys = proc->cr3;
        process_flush_user_space(pml4_phys);

        /* Liberta a raíz PML4 */
        pmm_free_page(pml4_phys);
    }
    
    /* 2. LIMPEZA DA ESTRUTURA E THREADS ASSOCIADAS */
    if (proc->main_thread != NULL) 
    {
        if (proc->main_thread->context_frame) 
        {
            void* stack_raw = (void*)proc->main_thread->context_frame_top;
            kfree(stack_raw);
        }
        
        // LIMPEZA DO VFS: Fecho regulamentar baseado em Contagem de Referências
        for (int i = 0; i < MAX_FILES_PER_PROCESS; i++)
        {
            if (proc->file_descriptor_table[i] != NULL)
            {
                /* 1. Decrementa a referência já que este processo está a largar o ficheiro */
                proc->file_descriptor_table[i]->ref_count--;

                /* 
                 * 2. DECISÃO DE DESTRUIÇÃO:
                 * O recurso só é destruído se mais NINGUÉM no sistema o estiver a usar.
                 */
                if (proc->file_descriptor_table[i]->ref_count == 0)
                {
                    kprintf("[VFS] ref_count == %d A fechar recurso partilhado globalmente: %s\n", 
                            proc->file_descriptor_table[i]->ref_count,
                            proc->file_descriptor_table[i]->node->name);
                    
                    /* Fecha o nó no sistema de ficheiros virtual */
                    vfs_close(proc->file_descriptor_table[i]->node);
                    
                    /* Liberta a estrutura do descritor de ficheiro da RAM */
                    kfree(proc->file_descriptor_table[i]);
                }
                else 
                {
                    kprintf("[VFS] Ficheiro mantido ativo. Outros processos ainda o utilizam (ref_count: %d).\n", 
                            proc->file_descriptor_table[i]->ref_count);
                }

                /* 3. Desvincula o ponteiro local deste processo morto por segurança */
                proc->file_descriptor_table[i] = NULL;
            }
        }

        kfree(proc->main_thread);

    }

    kprintf("[Process] Processo %d destruido com sucesso total.\n", proc->pid);
}

void process_exit(int code) {
    
    scheduler_exit(code);
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