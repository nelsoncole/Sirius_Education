/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: syscall.c
 *    Description: Implementação da System Call Table expandida com suporte total
 *                 ao VFS (E/S, Sincronização, Metadados, Remoção e Umount).
 *                 Controlo de concorrência isolado nas camadas de hardware.
 *
 *         Author: Nelson Cole
 *   Created Date: 05/09/2026
 *
 *    Modified By: Nelson Cole / AI Collaborator
 *  Modified Date: 15/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/syscall/syscall.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>
#include <kernel/kernel/sched/process.h>
#include <kernel/kernel/net/socket.h>
#include <kernel/kmods/kmod.h>
#include <kernel/kernel/sched/clone.h>
#include <kernel/kernel/sched/fork.h>
#include <kernel/arch/x86_64/kapi/timer.h>

/*
 * REGS DE HARDWARE ESPECÍFICOS DA ARQUITETURA (x86_64 MSRs)
 * ------------------------------------------------------------------------
 */
#define MSR_IA32_STAR 0xC0000081UL
#define MSR_IA32_LSTAR 0xC0000082UL
#define MSR_IA32_FMASK 0xC0000084UL

extern void syscall_entry_stub(void);

/* Definição de Otimização colocada no topo para evitar declarações implícitas */
#define unlikely(x)    __builtin_expect(!!(x), 0)

/*
 * ============================================================================
 * SYSTEM CALL TABLE (Vetor de Despacho Completo)
 * ============================================================================
 */
static const void *sys_call_table[MAX_SYSCALLS] = {
    /* Operações Base e VFS (Rodam com STI) */
    [SYS_READ]      = sys_read,
    [SYS_WRITE]     = sys_write,
    [SYS_MOUNT]     = sys_mount,
    [SYS_UMOUNT]    = sys_umount,
    [SYS_OPEN]      = sys_open,
    [SYS_CLOSE]     = sys_close,
    [SYS_SEEK]      = sys_seek,
    [SYS_FLUSH]     = sys_flush,
    [SYS_STAT]      = sys_stat,
    [SYS_CHMOD]     = sys_chmod,
    [SYS_UNLINK]    = sys_unlink,
    [SYS_RMDIR]     = sys_rmdir,
    [SYS_RENAME]    = sys_rename,
    [SYS_MKDIR]     = sys_mkdir,
    [SYS_GETDENTS]  = sys_getdents,
    [SYS_DUP2]      = sys_dup2,
    [SYS_IOCTL]     = sys_ioctl,

    /* Gestão de Memória Estrita (Rodam com CLI) */
    [SYS_BRK]       = sys_brk,
    [SYS_MMAP]      = sys_mmap,
    [SYS_MUNMAP]    = sys_munmap,

    /* Ciclo de Vida de Processos Estrito (Rodam com CLI) */
    [SYS_FORK]      = sys_fork,
    [SYS_EXECVE]    = sys_execve,
    [SYS_EXIT]      = sys_exit,
    [SYS_GETPID]    = sys_getpid,
    [SYS_GETPPID]   = sys_getppid,

    /* Sincronização, Tempo e Sinais (Rodam com STI) */
    [SYS_WAITPID]   = sys_waitpid,
    [SYS_SLEEP]     = sys_sleep,
    [SYS_USLEEP]    = sys_usleep,
    [SYS_KILL]      = sys_kill,
    [SYS_SIGACTION] = sys_sigaction,

    /* Subsistema de Sockets (Rodam com STI) */
    [SYS_SOCKET]     = sys_socket,
    [SYS_BIND]       = sys_bind,
    [SYS_LISTEN]     = sys_listen,
    [SYS_ACCEPT]     = sys_accept,
    [SYS_CONNECT]    = sys_connect,
    [SYS_SEND]       = sys_send,
    [SYS_RECV]       = sys_recv,
    [SYS_SENDTO]     = sys_sendto,
    [SYS_RECVFROM]   = sys_recvfrom,
    [SYS_SHUTDOWN]   = sys_shutdown,
    [SYS_SETSOCKOPT] = sys_setsockopt,
    [SYS_GETSOCKOPT] = sys_getsockopt,

    /* Identificação de Privilégios (UID / GID) */
    [SYS_GETUID]    = sys_getuid,
    [SYS_GETGID]    = sys_getgid,
    [SYS_SETUID]    = sys_setuid,
    [SYS_SETGID]    = sys_setgid,

    [SYS_KMOD_LOAD]  = sys_kmod_load,
    [SYS_KMOD_UNLOAD]= sys_kmod_unload,
    [SYS_KMOD_PRINT] = sys_kmod_print
};

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)(val & 0xFFFFFFFFFULL);
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ __volatile__("wrmsr" : : "c"(msr), "a"(low), "d"(high) : "memory");
}

/* 
 * Função auxiliar para classificar se a chamada de sistema é bloqueante/longa.
 * Retorna 1 se DEVE rodar com STI, ou 0 se deve rodar com CLI.
 */
static inline int syscall_is_blocking(uint64_t syscall_num) {
    if (unlikely(syscall_num >= MAX_SYSCALLS)) {
        return 0;
    }

    /* 
     * GRUPO: CLI ESTRITO (Retorna 0)
     * Estas syscalls lidam com estruturas internas altamente sensíveis do Kernel 
     * (memória, ciclo de vida de processos, agendamento e IDs).
     * NÃO PODEM sofrer preempção ou interrupções a meio da execução.
     */
    switch (syscall_num) {
        case SYS_BRK:
        case SYS_MMAP:
        case SYS_MUNMAP:
        case SYS_FORK:
        case SYS_EXECVE:
        case SYS_EXIT:
        case SYS_GETPID:
        case SYS_GETPPID:
            return 0; // GRUPO: CLI ESTRITO

        default:
            /* 
             * VFS, Rede, Sockets, Sinais e Timers entram aqui.
             * O caminho default é o mais executado (Branch Target Buffer otimizado).
             */
            return 1; // GRUPO: STI PERMITIDO
    }
}


uint64_t syscall_dispatcher(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    if (syscall_num >= MAX_SYSCALLS) {
        kprintf("[SCI Error] Chamada de sistema desconhecida: ID %ld\n", syscall_num);
        return (uint64_t)-1;
    }

    uint64_t (*handler)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) = (void *)sys_call_table[syscall_num];
    if (!handler) {
        kprintf("[SCI Error] Handler nulo para a syscall: ID %ld\n", syscall_num);
        return (uint64_t)-1;
    }

    // Verifica se esta syscall específica precisa de interrupções ativas
    int is_blocking = syscall_is_blocking(syscall_num);

    if (is_blocking) {
        interrupts_enable(); // Ativa interrupções (sti) ANTES de rodar o handler
    }

    // Executa a Syscall
    uint64_t result = handler(arg1, arg2, arg3, arg4, arg5, arg6);
    /* 
     * BARREIRA DE SEGURANÇA SEGUINTE:
     * Se as interrupções foram ativadas, TEMOS de as desativar antes de sair.
     * Se já estavam desativadas, isto garante que o estado se mantém seguro.
     */
    interrupts_disable(); // Desativa interrupções (cli)

    return result;
}

/*
 * ============================================================================
 * SERVIÇOS NATIVOS INTERNOS DO KERNEL (HANDLERS)
 * ============================================================================
 */

uint64_t sys_mount(const char* device_name, const char* mount_path, const char* fs_type) {
    return (uint64_t)vfs_mount(device_name, mount_path, fs_type);
}

uint64_t sys_umount(const char* mount_path) {
    return (uint64_t)vfs_umount(mount_path);
}

uint64_t sys_open(const char* path, uint32_t flags) {
    if (!path) return (uint64_t)-1;
    
    process_t* proc = get_current_process();
    if (!proc) return (uint64_t)-1;

    vfs_node_t* node = vfs_open(path, flags);
    if (!node) return (uint64_t)-1;

    int fd = -1;
    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
        if (proc->file_descriptor_table[i] == NULL) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        vfs_close(node);
        return (uint64_t)-2;
    }

    vfs_file_t* file = (vfs_file_t*)kmalloc(sizeof(vfs_file_t));
    if (!file) {
        vfs_close(node);
        return (uint64_t)-3;
    }

    file->node = node;
    file->offset = 0;
    file->flags = flags;
    proc->file_descriptor_table[fd] = file;

    return (uint64_t)fd;
}

uint64_t sys_close(int fd) {
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS) return (uint64_t)-1;

    process_t* proc = get_current_process();
    if (!proc || !proc->file_descriptor_table[fd]) return (uint64_t)-1;

    vfs_file_t* file = proc->file_descriptor_table[fd];
    vfs_close(file->node);
    kfree(file);
    proc->file_descriptor_table[fd] = NULL;

    return 0;
}

uint64_t sys_read(int fd, void* buffer, uint32_t size) {
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !buffer) return (uint64_t)-1;

    process_t* proc = get_current_process();
    if (!proc || !proc->file_descriptor_table[fd]) return (uint64_t)-1;

    vfs_file_t* file = proc->file_descriptor_table[fd];

    int bytes_lidos = vfs_read(file->node, file->offset, size, buffer);
    if (bytes_lidos > 0) {
        file->offset += bytes_lidos;
    }

    return (uint64_t)bytes_lidos;
}

uint64_t sys_write(int fd, const void* buffer, uint32_t size) {

    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !buffer) return (uint64_t)-1;

    process_t* proc = get_current_process();
    if (!proc || !proc->file_descriptor_table[fd]) return (uint64_t)-1;

    vfs_file_t* file = proc->file_descriptor_table[fd];

    int bytes_escritos = vfs_write(file->node, file->offset, size, (void*)buffer);
    if (bytes_escritos > 0) {
        file->offset += bytes_escritos;
    }

    return (uint64_t)bytes_escritos;
}

uint64_t sys_seek(int fd, int64_t offset, int whence) {
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS) return (uint64_t)-1;

    process_t* proc = get_current_process();
    if (!proc || !proc->file_descriptor_table[fd]) return (uint64_t)-1;

    vfs_file_t* file = proc->file_descriptor_table[fd];

    return vfs_seek(file, offset, whence);
}

uint64_t sys_flush(int fd) {
   if (fd < 0 || fd >= MAX_FILES_PER_PROCESS) return (uint64_t)-1;

    process_t* proc = get_current_process();
    if (!proc || !proc->file_descriptor_table[fd]) return (uint64_t)-1;

    vfs_file_t* file = proc->file_descriptor_table[fd];

    return (uint64_t)vfs_flush(file->node);
}

uint64_t sys_stat(const char* path, vfs_stat_t* buf) {
    if (!path || !buf) return (uint64_t)-1;

    vfs_node_t* node = vfs_open(path, 0); 
    if (!node) return (uint64_t)-1;
    
    int res = vfs_stat(node, buf);
    vfs_close(node);
    return (uint64_t)res;
}

uint64_t sys_chmod(const char* path, uint16_t mode) {
    if (!path) return (uint64_t)-1;

    vfs_node_t* node = vfs_open(path, 0);
    if (!node) return (uint64_t)-1;
    
    int res = vfs_chmod(node, mode);
    vfs_close(node);
    return (uint64_t)res;
}

/**
 * sys_unlink - Chamada de sistema para apagar ficheiros do VFS.
 */
uint64_t sys_unlink(const char* path) {
    if (!path || path[0] == '\0') return (uint64_t)-1;

    char file_name[128];
    // Resolve o parente real e isola apenas o nome do ficheiro (ex: "arqui")
    vfs_node_t* parent_node = vfs_get_parent_and_child(path, file_name);
    if (!parent_node) {
        return (uint64_t)-1;
    }

    // Invoca o motor interno passando o parente correto e o nome do alvo
    return (uint64_t)vfs_unlink(parent_node, file_name);
}

/**
 * sys_rmdir - Chamada de sistema para remover diretorias do VFS.
 */
uint64_t sys_rmdir(const char* path) {
    if (!path || path[0] == '\0') return (uint64_t)-1;

    char dir_name[128];
    // Resolve o parente real e isola apenas o nome da pasta (ex: "nova_pasta")
    vfs_node_t* parent_node = vfs_get_parent_and_child(path, dir_name);
    if (!parent_node) {
        return (uint64_t)-1;
    }

    // Invoca o motor interno passando o parente correto e o nome do alvo
    return (uint64_t)vfs_rmdir(parent_node, dir_name);
}

/**
 * sys_rename - Chamada de sistema para renomear um nó dentro do VFS.
 */
uint64_t sys_rename(const char* old_path, const char* new_path) {
    if (!old_path || old_path[0] == '\0' || !new_path || new_path[0] == '\0') {
        return (uint64_t)-1; // EINVAL
    }

    char old_name[128];
    char new_name[128];

    // 1. Resolve o diretório pai de ORIGEM e isola o nome antigo
    vfs_node_t* old_parent = vfs_get_parent_and_child(old_path, old_name);
    if (!old_parent) {
        return (uint64_t)-2; // ENOENT
    }

    // 2. Resolve o diretório pai de DESTINO e isola o novo nome
    vfs_node_t* new_parent = vfs_get_parent_and_child(new_path, new_name);
    if (!new_parent) {
        return (uint64_t)-2; // ENOENT
    }

    // ============================================================================
    // APLICADO AQUI:
    // Se new_name[0] for '\0', significa que o caminho terminava em '/'!
    // Logo, o utilizador quer apenas MOVER para dentro da pasta mantendo o nome.
    // ============================================================================
    if (new_name[0] == '\0') {
        // Herda o nome original do ficheiro para o destino
        strcpy(new_name, old_name);
    }

    /* 
     * AGORA SIM, INVOCAÇÃO DO MOTOR COM 4 ARGUMENTOS REAIS:
     * 1. Nó pai antigo | 2. Nome antigo | 3. Nó pai novo | 4. Novo nome
     */
    return (uint64_t)vfs_rename(old_parent, old_name, new_parent, new_name);
}

uint64_t sys_mkdir(const char* path, uint32_t mode) {
    char dir_name[64];

    vfs_node_t* parent_node = vfs_get_parent_and_child(path, dir_name);
    
    if (!parent_node || !parent_node->ops || !parent_node->ops->mkdir) {
        return (uint64_t)-1;
    }

    return (uint64_t)vfs_mkdir(parent_node, dir_name, mode);
}

/**
 * sys_exit - Encerra a execução do processo atual e liberta os seus recursos no VFS e no Scheduler.
 */
uint64_t sys_exit(uint64_t code) 
{
    kprintf("[SCI] sys_exit: code(%d)\n", code);
    int exit_code = (int)(code & 0xFFFFFFFF);

    /* 1. Captura o contexto estrutural do processo que está a morrer */
    cpu_data_block_t* cpu = get_current_cpu();
    thread_t* current_thread = cpu->current_thread;
    process_t* current_proc = current_thread->owner;

    if (!current_proc) {
        kprintf("[SCI ERROR] sys_exit: Processo atual nulo.\n");
        scheduler_exit(exit_code);
    }

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

    // 4. CHAMADA AO SCHEDULER: Passa o controlo definitivo da CPU.
    // Esta função assume o controlo da Stack e NUNCA mais retorna para esta linha!
    scheduler_exit(exit_code);

    // Linha de salvaguarda física
    while(1) { 
        __asm__ __volatile__("hlt"); 
    }
    return 0;
}

extern uint64_t brk(uint64_t new_break);
uint64_t sys_brk(void *addr) 
{
    uint64_t target_break = (uint64_t)addr;

    return brk(target_break);
}

/**
 * sys_ioctl - Chamada de sistema para controlo de dispositivos e operações especiais do VFS.
 * @fd:      Descritor de ficheiro do processo de Ring 3.
 * @request: Código de comando da operação (ex: 0x1001 para a árvore do VFS).
 * @arg:     Ponteiro opcional para argumentos ou buffers de dados.
 */
uint64_t sys_ioctl(int fd, unsigned long request, void *arg) {
    // 1. Obtém a estrutura do processo que invocou a Syscall a partir do escalonador
    process_t* proc = get_current_process();
    if (!proc || fd < 0 || fd >= MAX_FILES_PER_PROCESS) {
        return (uint64_t)-1; // EBADF: Descritor inválido
    }

    // 2. Extrai a estrutura física de controlo do ficheiro aberto
    vfs_file_t* file = proc->file_descriptor_table[fd];
    if (!file || !file->node) {
        return (uint64_t)-1; // EBADF: Ficheiro não aberto
    }

    vfs_node_t* node = file->node;
    (void)node;

    // Log de diagnóstico atómico (Mantive o teu formato original)
    kprintf("[SCI] sys_ioctl: fd=%d, req=0x%lx, arg=0x%lx\n", fd, request, (uint64_t)arg);

    // ============================================================================
    // INTEGRAÇÃO CRUCIAL DA ÁRVORE DO VFS (Comando Mágico 0x1001)
    // ============================================================================
    if (request == 0x1001) {
        // O utilizador passa no argumento 'arg' o caminho de início (ex: "/")
        const char* start_path = (const char*)arg;
        if (!start_path) {
            start_path = "/";
        }

        // Invoca de forma síncrona a tua função recursiva nativa do Kernel!
        // Ela varre os inodes e faz o kprintf direto na tty0 ativa.
        vfs_print_tree(start_path);
        return 0; // Sucesso absoluto
    }

    // ============================================================================
    // ROTEAMENTO POLIMÓRFICO PADRÃO PARA DRIVERS (PTY, TTY, AHCI, ETC.)
    // ============================================================================
    // Se o driver do dispositivo (como o teu vfs_pty.c) tiver a operação ioctl ativa:
    /*if (node->ops && node->ops->ioctl) {
        // Encaminha a execução para o driver físico tratar as flags do hardware
        return (uint64_t)node->ops->ioctl(node, request, arg);
    }*/

    // Se o driver não suportar comandos ioctl, retorna erro de operação não suportada (ENOTTY)
    return (uint64_t)-1; 
}

/**
 * sys_fork - Ponto de entrada oficial da chamada de sistema (Interface Void).
 *            Captura estritamente o RIP, RSP e RFLAGS essenciais da CPU.
 */
uint64_t sys_fork(void) 
{
    kprintf("[SCI] sys_fork: Capturando contexto completo do Pai para o Filho...\n");

    uint64_t kernel_stack_top = 0;
    uint64_t user_rsp = 0;

    /* 1. Captura os ponteiros estáveis da CPU guardados no segmento GS */
    __asm__ __volatile__("mov %%gs:0, %0" : "=r"(kernel_stack_top));
    __asm__ __volatile__("mov %%gs:8, %0" : "=r"(user_rsp));

    /* 2. stack_ptr aponta para o topo absoluto (Início dos pushes do seu Assembly) */
    uint64_t* stack_ptr = (uint64_t*)kernel_stack_top;

    /* 
     * 3. EXTRAÇÃO COMPLETA DA PILHA (Baseado estritamente no seu syscall_stub.asm):
     *   stack_ptr[-1] -> push r14 (RIP de retorno real)
     *   stack_ptr[-2] -> push r15 (RFLAGS originais)
     *   stack_ptr[-3] -> push rbp (Preserva o RBP legítimo)
     *   stack_ptr[-4] -> push rbx (Preserva o RBX legítimo)
     *   stack_ptr[-5] -> push r10 (Continha o antigo RSP ou argumento)
     *   stack_ptr[-6] -> push qword 0 (Padding de alinhamento)
     */
    uint64_t saved_rip    = stack_ptr[-1];
    uint64_t saved_rflags = stack_ptr[-2];
    uint64_t saved_rbp    = stack_ptr[-3];
    uint64_t saved_rbx    = stack_ptr[-4];
    uint64_t saved_r10    = stack_ptr[-5];

    /* 4. Sintetiza o frame local SEM ZERAR os outros registadores gerais */
    stack_frame_t frame;
    
    // Injeta os dados de controle de fluxo estáveis
    frame.rip    = saved_rip;
    frame.rsp    = user_rsp; // O RSP real capturado de gs:8
    frame.rflags = saved_rflags;
    
    // Preserva os registadores gerais do Pai para o Filho não acordar com zeros!
    frame.rbp    = saved_rbp;
    frame.rbx    = saved_rbx;
    frame.r10    = saved_r10;

    // Configura os seletores legítimos de Ring 3 da sua GDT
    frame.cs     = 0x2B; 
    frame.ss     = 0x23;

    /* 5. Executa o clone repassando o contexto 100% íntegro */
    pid_t pid = fork(&frame);

    return (uint64_t)pid;
}

uint64_t sys_execve(const char *pathname, char *const argv[], char *const envp[]) 
{
    kprintf("[SCI] sys_execve: Carregar executavel em %s\n", pathname);
    
    /* 
     * TODO: Chamar o seu subsistema 'process_loader.c' passando o pathname,
     * limpando o CR3 antigo e injetando a nova stack com argc/argv em Ring 3.
     */
    (void)argv;
    (void)envp;
    return 0;
}

uint64_t sys_mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset) 
{
    kprintf("[SCI] sys_mmap: addr=0x%lx, len=%lu, prot=%d, flags=%d\n", (uint64_t)addr, length, prot, flags);
    
    /* TODO: Alocar páginas virtuais no espaço de paginação do utilizador */
    (void)addr; (void)prot; (void)flags; (void)fd; (void)offset; (void)length;
    return 0;
}

uint64_t sys_munmap(void *addr, size_t length) 
{
    kprintf("[SCI] sys_munmap: Libertar addr=0x%lx, len=%lu\n", (uint64_t)addr, length);
    
    /* TODO: Desmapear as páginas e atualizar o TLB */
    (void)addr; (void)length;
    return 0;
}

uint64_t sys_getpid(void) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;
    
    kprintf("[SCI] sys_getpid: Consultar PID (Retorno: %d)\n", proc->pid);
    return (uint64_t)proc->pid;
}

uint64_t sys_getppid(void) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;
    
    kprintf("[SCI] sys_getppid: Consultar PPID (Retorno: %d)\n", proc->ppid);
    return (uint64_t)proc->ppid;
}

/**
 * sys_waitpid - Aguarda de forma síncrona que um processo filho mude de estado.
 * 
 * @pid:     O PID do filho desejado (-1 significa aguardar por QUALQUER filho).
 * @wstatus: Ponteiro de Ring 3 onde o Kernel injetará o código de terminação.
 * @options: Flags de controlo (ex: WNOHANG, embora aqui foquemos no bloqueio padrão).
 */
/**
 * sys_waitpid - Aguarda de forma síncrona que um processo filho mude de estado.
 *               Seguro para SMP: Protege as leituras/escritas da lista global.
 * 
 * @pid:     O PID do filho desejado (-1 significa aguardar por QUALQUER filho).
 * @wstatus: Ponteiro de Ring 3 onde o Kernel injetará o código de terminação.
 * @options: Flags de controlo (padrão POSIX).
 */
uint64_t sys_waitpid(int32_t pid, int *wstatus, int options) 
{
    kprintf("[SCI] sys_waitpid: Processo Pai (PID: %d) aguardando por Filho (PID: %d)\n", 
            get_current_cpu()->current_thread->owner->pid, pid);

    (void)options;

    // 1. Identifica o processo Pai atual
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* parent_proc = cpu->current_thread->owner;
    
    process_t* child_proc = NULL;

    /* REPETIÇÃO DE BUSCA SÍNCRONA (Loop de Bloqueio) */
    for (;;) 
    {
        child_proc = NULL;
        int tem_filhos_vivos = 0;

        /* 
         * BARREIRA SMP: Bloqueia o spinlock global antes de varrer a lista.
         * Isto impede que outra CPU remova ou adicione nós a meio da leitura.
         */
        process_list_spinlock_acquire();

        // 2. Varrer a lista global de processos para localizar o Filho legítimo
        for (process_t* p = g_process_list_head; p != NULL; p = p->next) 
        {
            // Garante a barreira de segurança: Só podemos esperar por filhos legítimos!
            if (p->ppid == parent_proc->pid) 
            {
                if (pid == -1 || p->pid == (uint32_t)pid)
                {
                    child_proc = p;
                    
                    if (p->state == PROCESS_ZOMBIE) 
                    {
                        // Encontrámos um filho que já terminou! Sair do loop de varredura
                        break;
                    }
                    
                    if (p->state != PROCESS_ZOMBIE) 
                    {
                        tem_filhos_vivos = 1;
                    }
                }
            }
        }

        /* Liberta temporariamente o trinco para permitir outras operações no Kernel */
        process_list_spinlock_release();

        // 3. CENÁRIO A: O Filho foi encontrado e já é um ZOMBIE (Limpeza e Coleta)
        if (child_proc && child_proc->state == PROCESS_ZOMBIE) 
        {
            pid_t child_pid = child_proc->pid;
            int status_final = child_proc->exit_code;

            /* Injeta o código de término no ponteiro do utilizador (Ring 3) */
            if (wstatus != NULL) 
            {
                // TODO: Idealmente, validar se o ponteiro 'wstatus' pertence à memória do user
                *wstatus = (status_final & 0xFF) << 8;
            }

            kprintf("[SCI] sys_waitpid: Filho PID %d recolhido. Removendo e destruindo...\n", child_pid);

            /* 
             * RECONCILIAÇÃO E REMOÇÃO EXCLUSIVA (Delegado ao sys_waitpid):
             * Primeiro removemos o processo da topologia global do sistema com segurança SMP.
             * De seguida, desabamos a árvore da MMU do utilizador e apagamos o PCB.
             */
            process_list_remove(child_proc); 
            process_destroy(child_proc);     

            return (uint64_t)child_pid; // Retorna o PID do filho limpo para o Pai
        }

        // 4. PROTEÇÃO: Se pedimos um PID específico e ele não é nosso filho nem existe
        if (!child_proc && !tem_filhos_vivos) 
        {
            kprintf("[SCI ERROR] sys_waitpid: PID %d nao e um filho valido ou nao existe.\n", pid);
            return (uint64_t)-1; // Erro POSIX: ECHILD
        }

        // 5. CENÁRIO B: O Filho existe mas ainda está a rodar -> Bloquear o Pai!
        kprintf("[SCI] sys_waitpid: Filho ainda ativo. Bloqueando Pai (PID: %d)...\n", parent_proc->pid);
        
        // Bloqueia preventivamente a thread associada a este processo pai
        thread_t* current_thread = cpu->current_thread;
        current_thread->state = THREAD_BLOCKED;

        /* 
         * FORÇA A TROCA DE CONTEXTO IMEDIATA:
         * Invoca o algoritmo de Scheduling para passar a vez a outra tarefa.
         * Quando o Pai for acordado pelo sys_exit do filho, ele reentrará no ciclo,
         * recolherá os dados do zombie e libertará a memória com sucesso.
         */
        schedule(); 
    }

    return (uint64_t)-1;
}

uint64_t sys_sleep(unsigned int seconds) 
{
    //kprintf("[SCI] sys_sleep: Colocar thread em repouso por %u segs\n", seconds);
    
    /* 
     * Converte segundos para microssegundos e delega à udelay estável do kernel.
     * Idealmente, no futuro, isto deve bloquear a thread no temporizador 
     * em vez de fazer busy-waiting na CPU.
     */
    mdelay((uint64_t)seconds * 1000);
    return 0;
}

uint64_t sys_usleep(unsigned int usec) 
{
    //kprintf("[SCI] sys_usleep: Colocar thread em repouso por %u microsegundos\n", usec);
    
    if (usec == 0) 
    {
        return 0;
    }

    /* 
     * Invoca a rotina estável de micro-atrasos por hardware do Kernel.
     * Esta função utiliza o temporizador calibrado (ex: ACPI PM Timer ou TSC) 
     * para reter a execução de forma precisa durante os microssegundos solicitados.
     */
    udelay((uint64_t)usec); 
    
    return 0;
}

uint64_t sys_kill(int32_t pid, int sig) 
{
    kprintf("[SCI] sys_kill: Enviar sinal %d para pid=%d\n", sig, pid);
    
    /* TODO: Localizar o PCB e empurrar o número do sinal na máscara pendente */
    (void)pid; (void)sig;
    return 0;
}

uint64_t sys_sigaction(int signum, const void *act, void *oldact) 
{
    kprintf("[SCI] sys_sigaction: Alterar acao do sinal %d\n", signum);
    (void)signum; (void)act; (void)oldact;
    return 0;
}

uint64_t sys_getuid(void) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;
    
    kprintf("[SCI] sys_getuid: Consultar UID do PID %d (Retorno: %u)\n", proc->pid, proc->uid);
    return (uint64_t)proc->uid;
}

uint64_t sys_getgid(void) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;
    
    kprintf("[SCI] sys_getgid: Consultar GID do PID %d (Retorno: %u)\n", proc->pid, proc->gid);
    return (uint64_t)proc->gid;
}

uint64_t sys_setuid(uid_t uid) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;
    
    kprintf("[SCI] sys_setuid: Alterar UID do PID %d de %u para %u\n", proc->pid, proc->uid, uid);
    
    /* 
     * BARREIRA DE SEGURANÇA BÁSICA:
     * Se o utilizador atual não for root (UID != 0), ele só pode mudar o UID
     * para ele próprio, impedindo a escalação ilegal de privilégios.
     */
    if (proc->uid != 0 && proc->uid != uid) 
    {
        kprintf("[SCI SECURITY] Falha: PID %d nao tem permissao para alterar UID.\n", proc->pid);
        return (uint64_t)-1; /* Retorna erro de operação não permitida (EPERM) */
    }

    proc->uid = uid;
    return 0; /* Sucesso */
}

uint64_t sys_setgid(gid_t gid) 
{
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;
    
    kprintf("[SCI] sys_setgid: Alterar GID do PID %d de %u para %u\n", proc->pid, proc->gid, gid);
    
    if (proc->uid != 0 && proc->gid != gid) 
    {
        kprintf("[SCI SECURITY] Falha: PID %d nao tem permissao para alterar GID.\n", proc->pid);
        return (uint64_t)-1;
    }

    proc->gid = gid;
    return 0;
}


uint64_t sys_socket(int domain, int type, int protocol) {

    // Encaminha e retorna o File Descriptor gerado
    return (uint64_t)socket(domain, type, protocol);
}

uint64_t sys_bind(int sockfd, const void *addr, uint32_t addrlen) {
   
    return (uint64_t)bind(sockfd, addr, (unsigned long)addrlen);
}

uint64_t sys_listen(int sockfd, int backlog) {
     
    return (uint64_t)listen(sockfd, backlog);
}

uint64_t sys_accept(int sockfd, void *addr, uint32_t *addrlen) {
   
    // Converte o ponteiro de uint32_t* para unsigned long* exigido pela rotina nativa
    return (uint64_t)accept(sockfd, addr, (unsigned long*)addrlen);
}

uint64_t sys_connect(int sockfd, const void *addr, uint32_t addrlen) {
    
    return (uint64_t)connect(sockfd, addr, (unsigned long)addrlen);
}

uint64_t sys_send(int sockfd, const void *buf, size_t len, int flags) {
   
    return (uint64_t)send(sockfd, buf, (unsigned long)len, flags);
}

uint64_t sys_recv(int sockfd, void *buf, size_t len, int flags) {
  
    return (uint64_t)recv(sockfd, buf, (unsigned long)len, flags);
}

/**
 * @brief NOVO: Syscall Sendto (Recepção de 6 argumentos da AMD64 ABI)
 */
uint64_t sys_sendto(int sockfd, const void* buf, size_t len, int flags, const void* dest_addr, uint64_t addrlen) {
                
    return (uint64_t)sendto(sockfd, buf, (unsigned long)len, flags, dest_addr, (unsigned long)addrlen);
}

uint64_t sys_recvfrom(int sockfd, void* buf, size_t len, int flags, void* src_addr, uint64_t* addrlen) {

    return (uint64_t)recvfrom(sockfd, buf, (unsigned long)len, flags, src_addr, (unsigned long*)addrlen);
}

uint64_t sys_shutdown(int sockfd, int how) {

    return (uint64_t)shutdown(sockfd, how);
}

uint64_t sys_setsockopt(int sockfd, int level, int optname, const void *optval, uint32_t optlen) {
    kprintf("[SCI] sys_setsockopt: sock=%d, lvl=%d, opt=%d, val=0x%lx, len=%u\n", sockfd, level, optname, (uint64_t)optval, optlen);
    return 0;
}

uint64_t sys_getsockopt(int sockfd, int level, int optname, void *optval, uint32_t *optlen) {
    kprintf("[SCI] sys_getsockopt: sock=%d, lvl=%d, opt=%d, val_ptr=0x%lx, len_ptr=0x%lx\n", sockfd, level, optname, (uint64_t)optval, (uint64_t)optlen);
    return 0;
}

uint64_t sys_kmod_load(const uint8_t *user_buffer, size_t size) {

    return (uint64_t)kmod_load(user_buffer, size);
}

uint64_t sys_kmod_unload(const char *user_name) {

    return (uint64_t)kmod_unload(user_name);
}

uint64_t sys_kmod_print(void) {
    kmod_print_all();
    return 0;
}


uint64_t sys_dup2(int oldfd, int newfd) {
    // Obtém o processo atual de forma segura para SMP baseando-se na CPU ativa
    cpu_data_block_t* cpu = get_current_cpu();
    if (!cpu || !cpu->current_thread) return -1;
    process_t* proc = cpu->current_thread->owner;

    // Delega a execução para a função core
    return k_dup2(proc, oldfd, newfd);
}

/*
 * ============================================================================
 * INTERFACE DE INICIALIZAÇÃO DE HARDWARE
 * ============================================================================
 */
void syscall_init(void) {
    /*
     * Habilitação de Extensões do Processador via MSR (EFER):
     * Evita a exceção 'Invalid Opcode' (#UD) ao ativar recursos avançados da CPU.
     *
     * Registador IA32_EFER (Extended Feature Enable Register) -> Endereço: 0xC0000080
     * - Bit 0  (SCE): System Call Extensions (Habilita as instruções SYSCALL/SYSRET).
     * - Bit 11 (NXE): No-Execute Enable (Habilita a proteção de páginas XD/NX).
     */
    uint32_t eax, edx;
    __asm__ __volatile__(
        "mov $0xC0000080, %%ecx\n"
        "rdmsr\n"
        : "=a"(eax), "=d"(edx) : : "ecx");

    eax |= (1U << 11); // Ativa o bit 11 (NXE)
    eax |= (1U << 0);  // Ativa o bit 0 (SCE)

    __asm__ __volatile__(
        "wrmsr\n"
        : : "a"(eax), "d"(edx), "c"(0xC0000080) : "memory");

    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x1B << 48);
    wrmsr(MSR_IA32_STAR, star);
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry_stub);
    wrmsr(MSR_IA32_FMASK, 0x200UL);
}