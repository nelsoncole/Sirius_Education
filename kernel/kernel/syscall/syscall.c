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
 *  Modified Date: 13/09/2026
 *
 *        License: MIT
 * ============================================================================
 */

#include <kernel/kernel/syscall/syscall.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>

#define MAX_OPEN_FILES_PER_PROCESS 32
static vfs_file_t* g_fd_table[MAX_OPEN_FILES_PER_PROCESS];

/*
 * REGS DE HARDWARE ESPECÍFICOS DA ARQUITETURA (x86_64 MSRs)
 * ------------------------------------------------------------------------
 */
#define MSR_IA32_STAR 0xC0000081UL
#define MSR_IA32_LSTAR 0xC0000082UL
#define MSR_IA32_FMASK 0xC0000084UL

extern void syscall_entry_stub(void);

/*
 * ============================================================================
 * SYSTEM CALL TABLE (Vetor de Despacho Completo)
 * ============================================================================
 */
static const void *sys_call_table[MAX_SYSCALLS] = {
    [SYS_MOUNT]   = sys_mount,
    [SYS_UMOUNT]  = sys_umount,
    [SYS_OPEN]    = sys_open,
    [SYS_CLOSE]   = sys_close,
    [SYS_READ]    = sys_read,
    [SYS_WRITE]   = sys_write,
    [SYS_SEEK]    = sys_seek,
    [SYS_FLUSH]   = sys_flush,
    [SYS_STAT]    = sys_stat,
    [SYS_CHMOD]   = sys_chmod,
    [SYS_UNLINK]  = sys_unlink,
    [SYS_RMDIR]   = sys_rmdir,
    [SYS_RENAME]  = sys_rename,
    [SYS_BRK]     = sys_brk,
    [SYS_EXIT]    = sys_exit
};

static inline void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t low = (uint32_t)(val & 0xFFFFFFFFFULL);
    uint32_t high = (uint32_t)(val >> 32);
    __asm__ __volatile__("wrmsr" : : "c"(msr), "a"(low), "d"(high) : "memory");
}

uint64_t syscall_dispatcher(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    if (syscall_num >= MAX_SYSCALLS) {
        kprintf("[SCI Error] Chamada de sistema desconhecida: ID %ld\n", syscall_num);
        return (uint64_t)-1;
    }

    uint64_t (*handler)(uint64_t, uint64_t, uint64_t) = (void *)sys_call_table[syscall_num];
    if (!handler) {
        kprintf("[SCI Error] Handler nulo para a syscall: ID %ld\n", syscall_num);
        return (uint64_t)-1;
    }

    return handler(arg1, arg2, arg3);
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

    vfs_node_t* node = vfs_open(path, flags);
    if (!node) return (uint64_t)-1;

    int fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES_PER_PROCESS; i++) {
        if (g_fd_table[i] == NULL) {
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
    g_fd_table[fd] = file;

    return (uint64_t)fd;
}

uint64_t sys_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES_PER_PROCESS || !g_fd_table[fd]) return (uint64_t)-1;

    vfs_file_t* file = g_fd_table[fd];
    vfs_close(file->node);
    kfree(file);
    g_fd_table[fd] = NULL;

    return 0;
}

uint64_t sys_read(int fd, void* buffer, uint32_t size) {
    if (fd < 0 || fd >= MAX_OPEN_FILES_PER_PROCESS || !g_fd_table[fd] || !buffer) return (uint64_t)-1;

    vfs_file_t* file = g_fd_table[fd];
    int bytes_lidos = vfs_read(file->node, file->offset, size, buffer);
    if (bytes_lidos > 0) {
        file->offset += bytes_lidos;
    }

    return (uint64_t)bytes_lidos;
}

uint64_t sys_write(int fd, const void* buffer, uint32_t size) {

    /*
     * NOTA: Apenas uma atralho do momento
     */
    for (uint64_t i = 0; i < size; i++)
    {
        const char *buf = (const char *)buffer;
        kprintf("%c", buf[i]);
    }

    if (fd < 0 || fd >= MAX_OPEN_FILES_PER_PROCESS || !g_fd_table[fd] || !buffer) return (uint64_t)-1;

    vfs_file_t* file = g_fd_table[fd];

    if (fd == 1 && file->node == NULL) { 
        const char* buf_str = (const char*)buffer;
        for (uint32_t i = 0; i < size; i++) kprintf("%c", buf_str[i]);
        return size;
    }

    int bytes_escritos = vfs_write(file->node, file->offset, size, (void*)buffer);
    if (bytes_escritos > 0) {
        file->offset += bytes_escritos;
    }

    return (uint64_t)bytes_escritos;
}

uint64_t sys_seek(int fd, int64_t offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES_PER_PROCESS || !g_fd_table[fd]) return (uint64_t)-1;

    return vfs_seek(g_fd_table[fd], offset, whence);
}

uint64_t sys_flush(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES_PER_PROCESS || !g_fd_table[fd]) return (uint64_t)-1;

    return (uint64_t)vfs_flush(g_fd_table[fd]->node);
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

uint64_t sys_brk(void *addr) {
    kprintf("[SCI] sys_brk: Solicitacao para expandir Heap ate 0x%lx\n", (uint64_t)addr);
    return 0;
}

/* Protótipo externo da função de saída do seu Scheduler */
extern void scheduler_exit(int code); 

/**
 * Encerra a execução do processo atual e liberta os seus recursos no VFS e no Scheduler.
 */
uint64_t sys_exit(uint64_t code) {
    // Converte o registador x86_64 de 64-bits para o tipo int esperado pelo Scheduler
    int exit_code = (int)(code & 0xFFFFFFFF);

    kprintf("[SCI] sys_exit: Processo encerrado com codigo %d\n", exit_code);

    // LIMPEZA DO VFS: Fecha todos os ficheiros que este processo deixou abertos para evitar memory leaks
    for (int i = 0; i < MAX_OPEN_FILES_PER_PROCESS; i++) {
        if (g_fd_table[i] != NULL) {
            vfs_close(g_fd_table[i]->node);
            kfree(g_fd_table[i]);
            g_fd_table[i] = NULL;
        }
    }

    // CHAMADA AO SCHEDULER: Altera o estado do processo e remove-o da fila de execução da CPU.
    // Esta função assume o controlo da Stack e NUNCA mais retorna para esta linha!
    scheduler_exit(exit_code);

    // Linha de salvaguarda física (Caso o scheduler falhe, a CPU não executa lixo)
    while(1) { 
        __asm__ __volatile__("hlt"); 
    }
    return 0;
}

/*
 * ============================================================================
 * INTERFACE DE INICIALIZAÇÃO DE HARDWARE
 * ============================================================================
 */
void syscall_init(void) {
    for (int i = 0; i < MAX_OPEN_FILES_PER_PROCESS; i++) {
        g_fd_table[i] = NULL;
    }

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

    /* ========================================================================
     * IA32_STAR
     *
     * Kernel:
     *
     *   CS = 0x08
     *
     * User SYSRET:
     *
     *   SS = 0x2B
     *   CS = 0x33
     *
     * GDT:
     *
     *   0x28 = Sysret Data
     *   0x30 = Sysret Code
     * ======================================================================== */

    uint64_t star = ((uint64_t)0x08 << 32) | ((uint64_t)0x23 << 48);
    wrmsr(MSR_IA32_STAR, star);
    wrmsr(MSR_IA32_LSTAR, (uint64_t)syscall_entry_stub);
    wrmsr(MSR_IA32_FMASK, 0x200UL);
}