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
 *    Modified By: Nelson Cole / AI Collaborator
 *  Modified Date: 13/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */
#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>
#include <kernel/drivers/storage/partitions.h>

#define FS_MAX_REG_DRIVERS 16

/* Tabelas Globais de Controlo do Subsistema */
static vfs_filesystem_t* g_registered_filesystems[FS_MAX_REG_DRIVERS];
static vfs_node_t*       g_vfs_root = NULL;

/* 
 * VARIÁVEL GLOBAL DE INICIALIZAÇÃO:
 * Armazena o nome literal da partição física de boot (ex: "ahci0.1")
 */
char g_boot_partition_name[32] = {0};

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
    
    vfs_release_lock(); // limpar spinloock

    // 1. Limpa o catálogo de drivers de sistemas de ficheiros
    for (int i = 0; i < FS_MAX_REG_DRIVERS; i++) {
        g_registered_filesystems[i] = NULL;
    }

    // 2. Cria o nó raiz primitivo '/' em memória RAM (Rootfs Elementar)
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
        kprintf("[VFS] CRÍTICO: Falha catastrofica ao alocar o no raiz do sistema.\n");
    }
}

/**
 * Localiza e identifica o dispositivo de boot fidedigno confrontando os metadados
 * de contexto guardados durante o MBR/GPT Scan com a assinatura física binária do UEFI.
 */
const char* vfs_identify_boot_partition(DEVICE_PATH_INFO* boot_device) {
    if (!boot_device || boot_device->PartitionSize == 0) {
        kprintf("[VFS BOOT] Erro: Dados da particao ativa de boot ausentes ou invalidos.\n");
        return NULL;
    }

    uint32_t target_part = (boot_device->PartitionNumber == 0) ? 1 : boot_device->PartitionNumber;

    kprintf("[VFS BOOT] A identificar particao via UEFI: ID .%u, LBA Inicial %lu\n", 
            target_part, boot_device->PartitionStart);

    for (int i = 0; i < 32; i++) {
        block_device_t* dev = block_get_device_by_index(i); 
        if (dev == NULL || dev->private_data == NULL) {
            continue; 
        }

        partition_ctx_t* ctx = (partition_ctx_t*)dev->private_data;

        if (ctx->partition_num == target_part && ctx->start_lba == boot_device->PartitionStart) {
            if (ctx->mbr_type == boot_device->MBRType && ctx->signature_type == boot_device->SignatureType) {
                if (memcmp(ctx->signature, boot_device->Signature, 16) == 0) {
                    const char* table_type_str = (ctx->mbr_type == 2) ? "GPT" : "MBR";
                    
                    strncpy(g_boot_partition_name, dev->name, sizeof(g_boot_partition_name) - 1);
                    g_boot_partition_name[sizeof(g_boot_partition_name) - 1] = '\0';

                    kprintf("[VFS BOOT] Match perfeito! Dispositivo resolvido de forma fidedigna: '%s' [%s]\n", 
                            g_boot_partition_name, table_type_str);
                    
                    return g_boot_partition_name; 
                }
            }
        }
    }

    g_boot_partition_name[0] = '\0';
    kprintf("[VFS BOOT] Aviso: Nenhuma particao em cache coincide com os dados binarios do UEFI.\n");
    
    return g_boot_partition_name;
}

/**
 * Varre iterativamente todo o catálogo de armazenamento global, localiza todas as
 * unidades de disco físicas brutas registadas e dispara o scanner síncrono MBR/GPT
 * para mapear dinamicamente todas as partições existentes na memória RAM.
 * 
 * @return 0 em caso de sucesso (pelo menos um disco processado), ou -1 se nenhum for encontrado.
 */
int vfs_init_partitions(void) {
    kprintf("[BOOT] A varrer o catalogo de armazenamento global a procura de particoes...\n");

    int devices_scanned = 0;

    // Varre todos os slots possíveis do catálogo de dispositivos de bloco (ex: limite de 16 ou 32)
    for (int i = 0; i < 32; i++) {
        //block_device_t* dev = block_get_device_by_index(i);
        block_device_t* dev = block_get_device_by_index(i);
        if (dev == NULL) continue;

        // CRITÉRIO DE ISOLAMENTO: Só escaneia se for um dispositivo físico bruto
        if (dev->is_raw == true) {
            kprintf("[BOOT] Unidade fisica '%s' detetada. A iniciar varredura MBR/GPT...\n", dev->name);
            
            // Faz o SCAN automático do disco bruto! Cria "ahci0.1", "ahci0.2", etc.
            partition_scan_device(dev);
            
            devices_scanned++;
        }
    }

    if (devices_scanned == 0) {
        kprintf("[BOOT] Erro Crítico: Nenhuma unidade física de armazenamento encontrada no catálogo.\n");
        return -1;
    }


    const char* boot_devi_name = vfs_identify_boot_partition(&g_boot_info->BootDevice);
    kprintf("[BOOT] Particao de inicializacao do sistema identificada: '%s'\n", boot_devi_name);
    kprintf("[BOOT] Varredura de partições concluida. %d unidade(s) fisica(s) mapeada(s).\n", devices_scanned);
    return 0; // Sucesso
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

    block_device_t* dev = NULL;
    if (device_name != NULL) {
        dev = block_get_device_by_name(device_name);
        if (!dev) {
            kprintf("[VFS MOUNT] Erro: Dispositivo '%s' nao encontrado.\n", device_name);
            return -2;
        }
    }

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

    if (strcmp(mount_path, "/") == 0) {
        vfs_acquire_lock();
        
        vfs_node_t* fs_root_node = fs_driver->mount(dev, mount_path);
        if (!fs_root_node) {
            kprintf("[VFS MOUNT] Erro: O driver '%s' falhou ao ler o dispositivo.\n", fs_type);
            vfs_release_lock();
            return -4;
        }

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
// OPERAÇÃO DE DESMONTAGEM (UMOUNT)
//-----------------------------------------------------------------------------
int vfs_umount(const char* mount_path) {
    if (!mount_path) return -1;

    // Nesta fase inicial, apenas a raiz '/' é suportada de forma simétrica ao vfs_mount
    if (strcmp(mount_path, "/") == 0) {
        vfs_acquire_lock();

        // Verifica se existe de facto um ponto de montagem ativo na raiz
        if (!(g_vfs_root->flags & VFS_MOUNTPOINT) || !g_vfs_root->ptr_mount) {
            kprintf("[VFS UMOUNT] Erro: Nao existe nenhum volume montado em '%s'.\n", mount_path);
            vfs_release_lock();
            return -2;
        }

        kprintf("[VFS UMOUNT] Desmontando volume da raiz '/'...\n");

        // 1. GARANTIA DE PERSISTÊNCIA: Força o flush final de metadados antes de desligar
        // Isto garante que o FAT32 salve tamanhos e buffers no HDD/SSD de forma definitiva
        if (g_vfs_root->ptr_mount->ops && g_vfs_root->ptr_mount->ops->flush) {
            g_vfs_root->ptr_mount->ops->flush(g_vfs_root->ptr_mount);
        }

        // 2. Desvincula o nó de hardware do rootfs virtual em RAM invocando o polimorfismo nativo
        if (g_vfs_root->ptr_mount->fs && g_vfs_root->ptr_mount->fs->unmount) {
            // O driver (ex: FAT32) limpa as suas estruturas privadas e dá kfree no nó raiz dele
            g_vfs_root->ptr_mount->fs->unmount(g_vfs_root->ptr_mount);
        } else {
            // Fallback genérico de segurança caso o driver ainda não implemente um unmount nativo
            kfree(g_vfs_root->ptr_mount);
        }
        
        g_vfs_root->ptr_mount = NULL;
        g_vfs_root->flags &= ~VFS_MOUNTPOINT;

        kprintf("[VFS UMOUNT] Sucesso: Volume removido com seguranca de '%s'.\n", mount_path);

        vfs_release_lock();
        return 0;
    }

    kprintf("[VFS UMOUNT] Erro: Pontos de desmontagem secundários ainda não são suportados.\n");
    return -3;
}

//-----------------------------------------------------------------------------
// OPERAÇÕES GENÉRICAS DE E/S (PROTEGIDAS CONTRA CONCORRÊNCIA SMP)
//-----------------------------------------------------------------------------
int vfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    if (!node || !buffer) return -1;
    
    vfs_acquire_lock();
    if ((node->flags & VFS_MOUNTPOINT) && node->ptr_mount) {
        node = node->ptr_mount;
    }

    if (node->ops && node->ops->read) {
        int res = node->ops->read(node, offset, size, buffer);
        vfs_release_lock();
        return res;
    }
    vfs_release_lock();
    return -2;
}

int vfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    if (!node || !buffer) return -1;

    vfs_acquire_lock();
    if ((node->flags & VFS_MOUNTPOINT) && node->ptr_mount) {
        node = node->ptr_mount;
    }

    if (node->ops && node->ops->write) {
        int res = node->ops->write(node, offset, size, buffer);
        vfs_release_lock();
        return res;
    }
    vfs_release_lock();
    return -2;
}

void vfs_close(vfs_node_t* node) {
    if (!node) return;
    
    vfs_acquire_lock();
    if (node->ops && node->ops->close) {
        node->ops->close(node);
    }
    vfs_release_lock();
}

int vfs_flush(vfs_node_t* node) {
    if (!node) return -1;
    
    vfs_acquire_lock();
    if ((node->flags & VFS_MOUNTPOINT) && node->ptr_mount) node = node->ptr_mount;
    if (node->ops && node->ops->flush) {
        int res = node->ops->flush(node);
        vfs_release_lock();
        return res;
    }
    vfs_release_lock();
    return -2;
}

int vfs_stat(vfs_node_t* node, vfs_stat_t* buf) {
    if (!node || !buf) return -1;
    
    vfs_acquire_lock();
    if ((node->flags & VFS_MOUNTPOINT) && node->ptr_mount) node = node->ptr_mount;
    if (node->ops && node->ops->stat) {
        int res = node->ops->stat(node, buf);
        vfs_release_lock();
        return res;
    }
    vfs_release_lock();
    return -2;
}

int vfs_chmod(vfs_node_t* node, uint16_t mode) {
    if (!node) return -1;
    
    vfs_acquire_lock();
    if ((node->flags & VFS_MOUNTPOINT) && node->ptr_mount) node = node->ptr_mount;
    if (node->ops && node->ops->chmod) {
        int res = node->ops->chmod(node, mode);
        vfs_release_lock();
        return res;
    }
    vfs_release_lock();
    return -2;
}

int vfs_unlink(vfs_node_t* parent, const char* name) {
    if (!parent || !name) return -1;
    
    vfs_acquire_lock();
    if ((parent->flags & VFS_MOUNTPOINT) && parent->ptr_mount) parent = parent->ptr_mount;
    if (parent->ops && parent->ops->unlink) {
        int res = parent->ops->unlink(parent, name);
        vfs_release_lock();
        return res;
    }
    vfs_release_lock();
    return -2;
}

int vfs_rmdir(vfs_node_t* parent, const char* name) {
    if (!parent || !name) return -1;
    
    vfs_acquire_lock();
    if ((parent->flags & VFS_MOUNTPOINT) && parent->ptr_mount) parent = parent->ptr_mount;
    if (parent->ops && parent->ops->rmdir) {
        int res = parent->ops->rmdir(parent, name);
        vfs_release_lock();
        return res;
    }
    vfs_release_lock();
    return -2;
}

int vfs_rename(vfs_node_t* parent, const char* old_name, const char* new_name) {
    if (!parent || !old_name || !new_name) return -1;
    
    vfs_acquire_lock();
    if ((parent->flags & VFS_MOUNTPOINT) && parent->ptr_mount) parent = parent->ptr_mount;
    if (parent->ops && parent->ops->rename) {
        int res = parent->ops->rename(parent, old_name, new_name);
        vfs_release_lock();
        return res;
    }
    vfs_release_lock();
    return -2;
}

//-----------------------------------------------------------------------------
// RESOLUÇÃO INTERATIVA DE CAMINHOS (TRAVAMENTO DE TOKENIZAÇÃO COMPLETO)
//-----------------------------------------------------------------------------
vfs_node_t* vfs_open(const char* path, uint32_t flags) {
    if (!path || path[0] == '\0') return NULL;

    vfs_acquire_lock(); // Garante exclusão mútua durante a busca e parsing da string

    vfs_node_t* current_node = g_vfs_root;
    if (g_vfs_root->flags & VFS_MOUNTPOINT) {
        current_node = g_vfs_root->ptr_mount;
    }

    if (strcmp(path, "/") == 0) {
        if (current_node->ops && current_node->ops->open) {
            if (current_node->ops->open(current_node, flags) != 0) {
                vfs_release_lock();
                return NULL; 
            }
        }
        vfs_release_lock();
        return current_node;
    }

    char path_copy[512];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char* token = path_copy;
    if (*token == '/') token++; 

    while (token && *token != '\0') {
        char* next_slash = strchr(token, '/');
        if (next_slash != NULL) {
            *next_slash = '\0';
        }

        if (!current_node->ops || !current_node->ops->finddir) {
            vfs_release_lock();
            return NULL;
        }

        vfs_node_t* next_node = current_node->ops->finddir(current_node, token);
        
        if (!next_node) {
            vfs_release_lock();
            return NULL;
        }

        if (current_node != g_vfs_root && current_node != g_vfs_root->ptr_mount) {
            kfree(current_node);
        }

        current_node = next_node;

        if (next_slash != NULL) {
            token = next_slash + 1;
        } else {
            break;
        }
    }

    if (current_node->ops && current_node->ops->open) {
        int res_open = current_node->ops->open(current_node, flags);
        if (res_open != 0) {
            if (current_node != g_vfs_root && current_node != g_vfs_root->ptr_mount) {
                kfree(current_node); 
            }
            vfs_release_lock();
            return NULL; 
        }
    }

    vfs_release_lock(); // Liberta o VFS de forma limpa para outros Cores usarem
    return current_node; 
}

//-----------------------------------------------------------------------------
// OPERAÇÃO DE POSICIONAMENTO DE PONTEIRO (SEEK)
//-----------------------------------------------------------------------------
/**
 * Altera a posição do ponteiro de leitura/escrita (offset) de um ficheiro aberto.
 * 
 * @param file   Ponteiro para a estrutura de sessão do ficheiro aberto.
 * @param offset Deslocamento em bytes (pode ser negativo para caminhos relativos).
 * @param whence Diretriz de posicionamento:
 *               VFS_SEEK_SET (0): Define o offset absoluto a partir do início.
 *               VFS_SEEK_CUR (1): Ajusta o offset relativo à posição atual.
 *               VFS_SEEK_END (2): Ajusta o offset relativo ao fim do ficheiro.
 * 
 * @return O novo offset absoluto em bytes a partir do início, ou -1 em caso de erro.
 */
uint64_t vfs_seek(vfs_file_t* file, int64_t offset, int whence) {
    if (!file || !file->node) {
        return (uint64_t)-1;
    }

    vfs_acquire_lock(); // Protege a alteração do offset lógico na RAM

    vfs_node_t* node = file->node;

    if ((node->flags & VFS_MOUNTPOINT) && node->ptr_mount) {
        node = node->ptr_mount;
    }

    uint64_t new_offset = file->offset;

    switch (whence) {
        case VFS_SEEK_SET:
            if (offset < 0) { vfs_release_lock(); return (uint64_t)-1; }
            new_offset = (uint64_t)offset;
            break;

        case VFS_SEEK_CUR:
            if (offset < 0 && ((uint64_t)(-offset) > file->offset)) {
                vfs_release_lock();
                return (uint64_t)-1;
            }
            new_offset = file->offset + offset;
            break;

        case VFS_SEEK_END:
            if (offset < 0 && ((uint64_t)(-offset) > node->size)) {
                vfs_release_lock();
                return (uint64_t)-1;
            }
            new_offset = node->size + offset;
            break;

        default:
            kprintf("[VFS SEEK] Erro: Diretriz 'whence' (%d) inválida.\n", whence);
            vfs_release_lock();
            return (uint64_t)-1;
    }

    // Proteção de sanidade (opcional): Impede que o ponteiro vá além dos limites físicos 
    // se o ficheiro for aberto estritamente em modo de leitura. Em modo de escrita, 
    // caminhos POSIX permitem fazer seek além do fim para criar "gaps" vazios (sparse files).
    if (!(file->flags & VFS_MODE_WRITE) && (new_offset > node->size)) {
        new_offset = node->size;
    }

    // Aplica a nova posição lógica
    file->offset = new_offset;

    vfs_release_lock();
    return file->offset;
}