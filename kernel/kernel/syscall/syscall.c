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
    [SYS_FSTAT]     = sys_fstat,
    [SYS_CHMOD]     = sys_chmod,
    [SYS_UNLINK]    = sys_unlink,
    [SYS_RMDIR]     = sys_rmdir,
    [SYS_RENAME]    = sys_rename,
    [SYS_MKDIR]     = sys_mkdir,
    [SYS_GETDENTS]  = sys_getdents,
    [SYS_DUP2]      = sys_dup2,
    [SYS_FCNTL]     = sys_fcntl,
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
    [SYS_KMOD_PRINT] = sys_kmod_print,

    [SYS_CHDIR]     = sys_chdir,
    [SYS_GETCWD]    = sys_getcwd
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

    
    /* 
     * ============================================================================
     * SALVAMENTO NA PILHA DE KERNEL LOCAL (VARIÁVEIS LOCAIS AUTOMÁTICAS)
     * ============================================================================
     * Estas variáveis residem na Stack de Kernel de 8KB da thread atual.
     * Mesmo que a thread mude de CPU ou sofra preempção, estes valores são 
     * imutáveis e isolados do resto do sistema.
     * ============================================================================
     */
    cpu_data_block_t* cpu_entry = get_current_cpu();
    uint64_t local_stack  = cpu_entry->user_stack;
    uint64_t local_rip    = cpu_entry->rip;
    uint64_t local_rflags = cpu_entry->rflag;


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
    
     /* 
     * Buscamos o bloco Per-CPU do núcleo ATUAL, pois a thread pode ter acordado
     * num core físico diferente daquele em que iniciou a chamada (Migração SMP).
     */
    cpu_data_block_t* cpu_exit = get_current_cpu();
    
    /*
     * Injeta de volta no bloco GS os valores puros que guardámos na stack de kernel 
     * no início da função. Qualquer alteração que o Filho ou a Idle Task tenham 
     * feito na estrutura 'cpu->user_stack' global do hardware é agora limpa.
     */
    cpu_exit->user_stack = local_stack;
    cpu_exit->rip        = local_rip;
    cpu_exit->rflag      = local_rflags;

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
    file->ref_count++;

    return (uint64_t)fd;
}

uint64_t sys_close(int fd) {
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS) return (uint64_t)-1;

    process_t* proc = get_current_process();
    if (!proc || !proc->file_descriptor_table[fd]) return (uint64_t)-1;

    vfs_file_t* file = proc->file_descriptor_table[fd];

    /* 1. DECREMENTA O CONTADOR DE REFERÊNCIAS EXISTENTE */
    file->ref_count--;

    /* 2. REMOVE O ACESSO DO PROCESSO ATUAL IMEDIATAMENTE */
    proc->file_descriptor_table[fd] = NULL;

    /* 3. SÓ DESTRÓI O NÓ SE ESTE FOR O ÚLTIMO PROCESSO A USÁ-LO */
    if (file->ref_count == 0) {
        if (file->node) {
            vfs_close(file->node);
        }
        kfree(file);
        kprintf("[VFS] Descritor destruido definitivamente (ref_count == 0).\n");
    } else {
        kprintf("[VFS] Descritor mantido vivo para outros processos. Restam: %u\n", file->ref_count);
    }

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

uint64_t sys_fstat(int fd, vfs_stat_t* buf) {
    // 1. Validação defensiva do descritor de ficheiro e do ponteiro do utilizador
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !buf) {
        return (uint64_t)-1;
    }

    // 2. Resgata o processo dono da thread ativa na CPU
    process_t* proc = get_current_process();
    if (!proc || !proc->file_descriptor_table[fd]) {
        return (uint64_t)-1;
    }

    // 3. Extrai o objeto de ficheiro do processo
    vfs_file_t* file = proc->file_descriptor_table[fd];
    if (!file || !file->node) {
        return (uint64_t)-1;
    }

    int res = vfs_stat(file->node, buf);

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
    __asm__ __volatile__("movq %%gs:0, %0" : "=r"(kernel_stack_top));
    __asm__ __volatile__("movq %%gs:8, %0" : "=r"(user_rsp));

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
    uint64_t saved_rip    = 0;
    uint64_t saved_rflags = 0;

    __asm__ __volatile__("movq %%gs:16, %0" : "=r"(saved_rip));
    __asm__ __volatile__("movq %%gs:24, %0" : "=r"(saved_rflags));

    uint64_t saved_rbp    = stack_ptr[-1];
    uint64_t saved_rbx    = stack_ptr[-2];
    uint64_t saved_r10    = stack_ptr[-3];

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

extern int elf_load_and_execve(const char* path, int argc, char** argv, process_t* proc);
uint64_t sys_execve(const char *pathname, char *const argv[], char *const envp[]) 
{
    if (!pathname) return -1;

    kprintf("[SCI] sys_execve: Carregar executavel em %s\n", pathname);

    // 1. Contar argumentos (argv) e variáveis (envp)
    char *const *argv_p = argv;
    char *init_argv[] = {
        ".",
        NULL
    };

    int argc = 0; 
    if (argv) { while (argv[argc] != NULL) argc++; }
    else {argc = 1; argv_p = init_argv;}

    int envc = 0; 
    if (envp) { while (envp[envc] != NULL) envc++; }

    // 2. Resgate direto do processo dono da thread atual na CPU
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;
    if (!proc){
        return -1;
    }

    // 3. Passa o caminho, argc, argv e a estrutura do processo para o carregador
    int status = elf_load_and_execve(pathname, argc, (char**)argv_p, proc);
    if (status != 0) return -1;

    /* 
     * 4. CONSTRUÇÃO E LIMPEZA DO CONTEXTO DE RETORNO (RING 3)
     * Intercepta a stack de kernel da thread principal. O iretq do scheduler_yield_execve()
     * vai descarregar este frame e enviar o CPU diretamente para o Ring 3.
     */
    thread_t* thread = proc->main_thread;
    thread->context_frame = (void*)((uint64_t)thread->context_frame_top - sizeof(stack_frame_t));
    thread->kernel_stack_top = (void*)thread->context_frame_top;

    // Mapeia o topo da stack diretamente na tua estrutura oficial de registos
    stack_frame_t* frame = (stack_frame_t*)thread->context_frame;
    
    // Configuração estrita dos registos de controlo e segmentação x86_64
    frame->rip        = proc->code_base;   // Entry point determinado pelo parser ELF
    frame->cs         = 0x2B;              // User Code Selector (RPL 3)
    frame->rsp        = proc->stack_top;   // Nova stack Ring 3 montada com os argumentos
    frame->ss         = 0x23;              // User Data Selector (RPL 3)
    frame->rflags     = 0x202;             // IF=1 (Interrupções ativas ao retornar para o utilizador)
    frame->int_no     = 32;
    frame->error_code = 0;

    // BLINDAGEM: Zera todos os registos gerais para o novo programa iniciar limpo
    frame->rax = 0; frame->rbx = 0; frame->rcx = 0; frame->rdx = 0;
    frame->rsi = 0; frame->rdi = 0; frame->rbp = 0;
    frame->r8  = 0; frame->r9  = 0; frame->r10 = 0; frame->r11 = 0;
    frame->r12 = 0; frame->r13 = 0; frame->r14 = 0; frame->r15 = 0;

    __asm__ __volatile__ (
        "movq %0, %%rsp\n\t"
        "movq %1, %%rcx\n\t"
        "movq %2, %%r11\n\t"
        "swapgs\n\t"         
        "sysretq"         
        :
        : "r" (frame->rsp),
          "r" (frame->rip),
          "r" (frame->rflags)
        : "rcx", "r11", "memory"
    );

    // 5. Chuta de forma supersónica para o escalonador sem olhar para trás
    //scheduler_yield_execve((uint64_t)frame);

    return 0; // Inalcançável
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
    (void)options;

    cpu_data_block_t* cpu = get_current_cpu();
    process_t* parent_proc = cpu->current_thread->owner;
    if (!parent_proc) return (uint64_t)-1;
    
    process_t* child_proc = NULL;

    kprintf("[SCI] sys_waitpid: Processo Pai (PID: %d) aguardando por Filho (PID: %d)\n", 
            parent_proc->pid, pid);

    for (;;) 
    {
        child_proc = NULL;
        int tem_filhos_vivos = 0;
        process_t* zombie_encontrado = NULL;

        process_list_spinlock_acquire();

        // 2. VARREDURA COMPLETA: Registamos o estado real de toda a descendência
        for (process_t* p = g_process_list_head; p != NULL; p = p->next) 
        {
            if (p->ppid == parent_proc->pid) 
            {
                if (pid == -1 || p->pid == (uint32_t)pid)
                {
                    if (p->state == PROCESS_ZOMBIE) 
                    {
                        zombie_encontrado = p; // Regista o ponteiro do zombie candidato
                    }
                    else 
                    {
                        tem_filhos_vivos = 1;  // Existem outros irmãos ainda ativos em Ring 3
                    }
                }
            }
        }

        process_list_spinlock_release();

        // Se encontrámos um zombie estável, elegemo-lo para destruição e coleta
        if (zombie_encontrado != NULL) {
            child_proc = zombie_encontrado;
        }

        // 3. CENÁRIO A: Limpeza e Coleta do Zombie
        if (child_proc && child_proc->state == PROCESS_ZOMBIE) 
        {
            pid_t child_pid = child_proc->pid;
            int status_final = child_proc->exit_code;

            if (wstatus != NULL) 
            {
                if ((uintptr_t)wstatus < 0x00007FFFFFFFF000UL) {
                    *wstatus = (status_final & 0xFF) << 8;
                }
            }

            kprintf("[SCI] sys_waitpid: Filho PID %d recolhido. Removendo...\n", child_pid);

            process_list_remove(child_proc);
            kfree(child_proc);    
            return (uint64_t)child_pid; 
        }

        // 4. PROTEÇÃO DEFENSIVA: Evita loops infinitos se o PID pedido não existir
        if (!child_proc && !tem_filhos_vivos) 
        {
            kprintf("[SCI ERROR] sys_waitpid: PID %d nao e um filho valido ou nao possui descendentes ativos.\n", pid);
            return (uint64_t)-1; 
        }

        // 5. CENÁRIO B: Bloqueio estruturado e atómico da Thread do Pai
        kprintf("[SCI] sys_waitpid: Filho ativo. Bloqueando Pai (PID: %d)...\n", parent_proc->pid);
        
        thread_t* current_thread = cpu->current_thread;
        current_thread->state = THREAD_BLOCKED;

        schedule(); // O Pai adormece e cede os ciclos de CPU de forma limpa.
        
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

uint64_t sys_fcntl(int fd, uint32_t cmd, uint64_t arg) {
#define O_APPEND    0x0400
#define O_NONBLOCK  0x0800
#define F_DUPFD     0
#define F_GETFD     1
#define F_SETFD     2
#define F_GETFL     3
#define F_SETFL     4
    // 1. Validação de sanidade do descritor de ficheiro (idêntica ao sys_read)
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS) {
        return (uint64_t)-1;
    }

    // 2. Obtém o processo atual em execução no núcleo
    process_t* proc = get_current_process();
    if (!proc || !proc->file_descriptor_table[fd]) {
        return (uint64_t)-1;
    }

    // 3. Extrai o ficheiro aberto guardado na tabela do processo
    vfs_file_t* file = proc->file_descriptor_table[fd];
    if (!file) {
        return (uint64_t)-1;
    }

    // 4. Processamento dos comandos enviados pelo Ring 3
    switch (cmd) {
        case F_GETFL:
            // Retorna as flags atuais do ficheiro (ex: O_RDWR, O_APPEND)
            return (uint64_t)file->flags;

        case F_SETFL:
            // Define novas flags de estado (restringindo a flags modificáveis como O_NONBLOCK e O_APPEND)
            // Essencial para o funcionamento correto do socket não-bloqueante no daemon TLSe
            file->flags = (file->flags & ~O_NONBLOCK) | (arg & O_NONBLOCK);
            file->flags = (file->flags & ~O_APPEND)   | (arg & O_APPEND);
            return 0;

        case F_GETFD:
            // Retorna as flags do descritor (ex: FD_CLOEXEC)
            return (uint64_t)file->fd_flags;

        case F_SETFD:
            // Define as flags do descritor
            file->fd_flags = (uint32_t)arg;
            return 0;

        default:
            // Comando desconhecido ou não implementado no VFS
            return (uint64_t)-1;
    }
}

#define ENOENT 2        /* No such file or directory */
#define ENOTDIR 20      /* Not a directory */
#define ERANGE 34       /* Result too large (Buffer do utilizador é demasiado pequeno) */
#define ENAMETOOLONG 36 /* File name too long */
/**
 * Interface da Chamada de Sistema chdir (sys_chdir).
 * Altera o diretório de trabalho atual (PWD) do processo ativo.
 * 
 * @param path Caminho absoluto ou relativo do diretório alvo.
 * @return Retorna 0 em caso de sucesso, ou um código de erro negativo.
 */
uint64_t sys_chdir(const char *path)
{
    if (!path) return -ENOENT;

    // 1. Resgata o processo atual dono da thread na CPU
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;
    if (!proc) return -1;

    // 2. Proteção defensiva: verifica se o tamanho do caminho cabe no nosso limite de 256 bytes
    size_t path_len = strlen(path);
    if (path_len >= MAX_PATH_LENGTH) {
        return -ENAMETOOLONG;
    }

    /* 
     * 3. VALIDAÇÃO NO VFS
     * Tentamos abrir o caminho em modo de leitura para validar a sua existência.
     * Nota: Se o teu VFS exigir caminhos absolutos, assume-se que 'path' começa com '/'.
     */
    vfs_node_t* dir_node = vfs_open(path, VFS_MODE_READ);
    if (!dir_node) {
        kprintf("[Process] Erro: sys_chdir falhou. Caminho '%s' nao existe.\n", path);
        return -ENOENT;
    }

    // 4. Verifica se o nó do VFS é realmente um diretório
    if ((dir_node->flags & VFS_DIRECTORY) == 0) {
        kprintf("[Process] Erro: sys_chdir falhou. '%s' nao e um diretorio.\n", path);
        vfs_close(dir_node);
        return -ENOTDIR;
    }

    // Fecha o nó imediatamente, pois só precisávamos de validar a sua existência física
    vfs_close(dir_node);

    /* 
     * 5. ATUALIZAÇÃO SEGURA DO PWD
     * Copia o novo caminho validado para o buffer de 256 bytes do PCB.
     * O uso de kstrncpy garante que nunca estouraremos os limites da estrutura.
     */
    memset(proc->pwd, 0, MAX_PATH_LENGTH);
    strncpy(proc->pwd, path, path_len);

    kprintf("[Process] Processo %d mudou o PWD para: '%s'\n", proc->pid, proc->pwd);

    return 0; // Sucesso
}

/**
 * Interface da Chamada de Sistema getcwd (sys_getcwd).
 * Copia o diretório de trabalho atual (PWD) do processo ativo para o buffer do utilizador.
 * 
 * @param buf  Ponteiro para o buffer no User Space.
 * @param size Tamanho máximo do buffer alocado pelo utilizador.
 * @return Retorna 0 em caso de sucesso, ou um código de erro negativo.
 */
uint64_t sys_getcwd(char *buf, size_t size)
{
    // 1. Validação defensiva inicial do buffer e tamanho fornecido
    if (!buf || size == 0) return -EINVAL;

    // 2. Resgata o processo atual dono da thread na CPU
    cpu_data_block_t* cpu = get_current_cpu();
    process_t* proc = cpu->current_thread->owner;
    if (!proc) return -1;

    // 3. Verifica o comprimento real do caminho guardado no PCB
    size_t pwd_len = strlen(proc->pwd) + 1; // +1 para incluir o terminador '\0'

    // Se o buffer do utilizador for menor do que o caminho real, falha com ERANGE
    if (size < pwd_len) {
        kprintf("[Process] Erro: sys_getcwd falhou. Buffer do utilizador (%lu bytes) e pequeno para '%s' (%lu bytes).\n", 
                size, proc->pwd, pwd_len);
        return -ERANGE;
    }

    memcpy(buf, proc->pwd, pwd_len);

    kprintf("[Process] sys_getcwd: Caminho '%s' copiado com sucesso para o processo %d.\n", proc->pwd, proc->pid);

    return 0; // Sucesso (A libc receberá 0 e retornará o ponteiro do buffer ao utilizador)
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