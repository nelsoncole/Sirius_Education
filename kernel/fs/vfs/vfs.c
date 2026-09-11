/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs.c
 *    Description: Implementação do Sistema de Ficheiros Virtual (VFS).
 *                 Gere a árvore de diretórios raiz, o catálogo de drivers
 *                 de ficheiros registados e a resolução de caminhos POSIX.
 * 
 *         Author: Nelson Cole
 *   Created Date: 11/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 11/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>

#define FS_MAX_REG_DRIVERS 16

/* Tabelas Globais de Controlo do Subsistema */
static vfs_filesystem_t* g_registered_filesystems[FS_MAX_REG_DRIVERS];
static vfs_node_t*       g_vfs_root = NULL;

/* Spinlock elementar para proteção de escrita na árvore (Útil para multiprocessamento SMP) */
static volatile uint64_t vfs_lock = 0;

static void vfs_acquire_lock(void) {
    while (__atomic_test_and_set(&vfs_lock, __ATOMIC_ACQUIRE)) {
        __asm__ __volatile__("pause" ::: "memory");
    }
}

static void vfs_release_lock(void) {
    __atomic_clear(&vfs_lock, __ATOMIC_RELEASE);
}

//-----------------------------------------------------------------------------
// INICIALIZAÇÃO DO SUBSISTEMA VFS
//-----------------------------------------------------------------------------
void vfs_init(void) {
    vfs_acquire_lock();

    // 1. Limpa o catálogo de drivers de sistemas de ficheiros
    for (int i = 0; i < FS_MAX_REG_DRIVERS; i++) {
        g_registered_filesystems[i] = NULL;
    }

    // 2. Cria o nó raiz primitivo '/' em memória RAM (Rootfs Elementar)
    // Nota: Substitua kmalloc pelo alocador real do seu Heap
    g_vfs_root = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (g_vfs_root) {
        memset(g_vfs_root, 0, sizeof(vfs_node_t));
        memcpy(g_vfs_root->name, "/", 2);
        g_vfs_root->flags = VFS_DIRECTORY;
        g_vfs_root->size = 0;
        g_vfs_root->inode = 0;
        g_vfs_root->ops = NULL; // Sem operações de disco ainda (puramente em RAM)
        g_vfs_root->fs = NULL;
        g_vfs_root->ptr_mount = NULL;
        
        kprintf("[VFS] Virtual File System Root '/' inicializado com sucesso em RAM.\n");
    } else {
        kprintf("[VFS] CRÍTICO: Falha catastrófica ao alocar o nó raiz do sistema.\n");
    }

    vfs_release_lock();
}

//-----------------------------------------------------------------------------
// REGISTO DE DRIVERS DE SISTEMAS DE FICHEIROS
//-----------------------------------------------------------------------------
int vfs_register_filesystem(vfs_filesystem_t* fs) {
    if (!fs || !fs->name) return -1;

    vfs_acquire_lock();
    for (int i = 0; i < FS_MAX_REG_DRIVERS; i++) {
        if (g_registered_filesystems[i] == NULL) {
            g_registered_filesystems[i] = fs;
            kprintf("[VFS] Driver de Sistema de Ficheiros '%s' registado.\n", fs->name);
            vfs_release_lock();
            return 0;
        }
    }
    vfs_release_lock();
    return -2; // Catálogo de drivers cheio
}

//-----------------------------------------------------------------------------
// OPERAÇÃO DE MONTAGEM (MOUNT)
//-----------------------------------------------------------------------------
int vfs_mount(const char* device_name, const char* mount_path, const char* fs_type) {
    if (!mount_path || !fs_type) return -1;

    // 1. Localiza o dispositivo de bloco no catálogo global (block.h)
    block_device_t* dev = NULL;
    if (device_name != NULL) {
        dev = block_get_device_by_name(device_name);
        if (!dev) {
            kprintf("[VFS MOUNT] Erro: Dispositivo '%s' nao encontrado.\n", device_name);
            return -2;
        }
    }

    // 2. Localiza o driver do sistema de ficheiros pretendido
    vfs_filesystem_t* fs_driver = NULL;
    for (int i = 0; i < FS_MAX_REG_DRIVERS; i++) {
        if (g_registered_filesystems[i] != NULL && strcmp(g_registered_filesystems[i]->name, fs_type) == 0) {
            fs_driver = g_registered_filesystems[i];
            break;
        }
    }

    if (!fs_driver) {
        kprintf("[VFS MOUNT] Erro: Driver de FS '%s' nao esta registado.\n", fs_type);
        return -3;
    }

    // 3. Resolve o ponto de montagem na árvore para injetar o disco (apenas '/' suportado por agora)
    if (strcmp(mount_path, "/") == 0) {
        vfs_acquire_lock();
        
        // Invoca o callback de montagem específico do driver (ex: fat_mount)
        vfs_node_t* fs_root_node = fs_driver->mount(dev, mount_path);
        if (!fs_root_node) {
            kprintf("[VFS MOUNT] Erro: O driver '%s' falhou ao ler o dispositivo.\n", fs_type);
            vfs_release_lock();
            return -4;
        }

        // Intervém na raiz: Transforma a raiz virtual no nó raiz real do disco montado
        g_vfs_root->ptr_mount = fs_root_node;
        g_vfs_root->flags |= VFS_MOUNTPOINT;
        
        kprintf("[VFS MOUNT] Sucesso: '%s' montado em '%s' usando '%s'.\n", 
                device_name ? device_name : "Virtual", mount_path, fs_type);
        
        vfs_release_lock();
        return 0;
    }

    kprintf("[VFS MOUNT] Erro: Pontos de montagem secundários ainda não são suportados.\n");
    return -5;
}

//-----------------------------------------------------------------------------
// OPERAÇÕES GENÉRICAS DE E/S (EXPOSTAS ÀS SYSCALLS)
//-----------------------------------------------------------------------------
int vfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    if (!node || !buffer) return -1;
    
    // Se o nó estiver redirecionado para um ponto de montagem ativo
    if ((node->flags & VFS_MOUNTPOINT) && node->ptr_mount) {
        node = node->ptr_mount;
    }

    if (node->ops && node->ops->read) {
        return node->ops->read(node, offset, size, buffer);
    }
    return -2; // Operação não suportada por este nó
}

int vfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    if (!node || !buffer) return -1;

    if ((node->flags & VFS_MOUNTPOINT) && node->ptr_mount) {
        node = node->ptr_mount;
    }

    if (node->ops && node->ops->write) {
        return node->ops->write(node, offset, size, buffer);
    }
    return -2;
}

vfs_node_t* vfs_open(const char* path, uint32_t flags) {
    (void)flags;
    // Se o pedido for a própria raiz, devolve a raiz montada ou a primitiva
    if (strcmp(path, "/") == 0) {
        if (g_vfs_root->flags & VFS_MOUNTPOINT) {
            return g_vfs_root->ptr_mount;
        }
        return g_vfs_root;
    }

    // Proxima fase: Implementar o Path Parsing interativo (Tokenização de '/')
    return NULL; 
}

void vfs_close(vfs_node_t* node) {
    if (node && node->ops && node->ops->close) {
        node->ops->close(node);
    }
}