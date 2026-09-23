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

uint64_t sys_unlink(const char* path) {
    if (!path) return (uint64_t)-1;

    vfs_node_t* parent = vfs_open("/", 0); 
    return (uint64_t)vfs_unlink(parent, path);
}

uint64_t sys_rmdir(const char* path) {
    if (!path) return (uint64_t)-1;

    vfs_node_t* parent = vfs_open("/", 0);
    return (uint64_t)vfs_rmdir(parent, path);
}

uint64_t sys_rename(const char* old_path, const char* new_name) {
    if (!old_path || !new_name) return (uint64_t)-1;

    vfs_node_t* parent = vfs_open("/", 0);
    return (uint64_t)vfs_rename(parent, old_path, new_name);
}

/**
 * Encerra a execução do processo atual e liberta os seus recursos no VFS e no Scheduler.
 */
uint64_t sys_exit(uint64_t code) {
    
    // Converte o registador x86_64 de 64-bits para o tipo int esperado pelo Scheduler
    int exit_code = (int)(code & 0xFFFFFFFF);

    //kprintf("[SCI] sys_exit: Processo encerrado com codigo %d\n", exit_code);

    // CHAMADA AO SCHEDULER: Altera o estado do processo e remove-o da fila de execução da CPU.
    // Esta função assume o controlo da Stack e NUNCA mais retorna para esta linha!
    scheduler_exit(exit_code);

    // Linha de salvaguarda física (Caso o scheduler falhe, a CPU não executa lixo)
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

uint64_t sys_ioctl(int fd, unsigned long request, void *arg) {
    (void)request;
    (void)arg;
    kprintf("[SCI] sys_ioctl: fd=%d, req=0x%lx, arg=0x%lx\n", fd, request, (uint64_t)arg);
    return 0;
}

uint64_t sys_fork(void) {
    kprintf("[SCI] sys_fork: Clonar processo atual\n");
    return 0;
}

uint64_t sys_execve(const char *pathname, char *const argv[], char *const envp[]) {
    (void)pathname;
    (void)argv;
    (void)envp;
    kprintf("[SCI] sys_execve: Carregar executavel 0x%lx\n", (uint64_t)pathname);
    return 0;
}

uint64_t sys_mmap(void *addr, size_t length, int prot, int flags, int fd, int64_t offset) {
    (void)addr;
    (void)prot;
    (void)flags;
    (void)fd;
    (void)offset;
    kprintf("[SCI] sys_mmap: addr=0x%lx, len=%lu, prot=%d, flags=%d\n", (uint64_t)addr, length, prot, flags);
    return 0;
}

uint64_t sys_munmap(void *addr, size_t length) {
    (void)addr;
    (void)length;
    kprintf("[SCI] sys_munmap: Libertar addr=0x%lx, len=%lu\n", (uint64_t)addr, length);
    return 0;
}

uint64_t sys_getpid(void) {
    kprintf("[SCI] sys_getpid: Consultar PID\n");
    return 0;
}

uint64_t sys_getppid(void) {
    kprintf("[SCI] sys_getppid: Consultar PID do Pai\n");
    return 0;
}

uint64_t sys_waitpid(int32_t pid, int *wstatus, int options) {
    (void)pid;
    (void)wstatus;
    (void)options;
    kprintf("[SCI] sys_waitpid: Aguardar por pid=%d\n", pid);
    return 0;
}

uint64_t sys_sleep(unsigned int seconds) {
    (void)seconds;
    kprintf("[SCI] sys_sleep: Colocar thread em repouso por %u segs\n", seconds);
    return 0;
}

uint64_t sys_kill(int32_t pid, int sig) {
    (void)pid;
    (void)sig;
    kprintf("[SCI] sys_kill: Enviar sinal %d para pid=%d\n", sig, pid);
    return 0;
}

uint64_t sys_sigaction(int signum, const void *act, void *oldact) {
    kprintf("[SCI] sys_sigaction: Alterar acao do sinal %d (act=0x%lx, old=0x%lx)\n", signum, (uint64_t)act, (uint64_t)oldact);
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