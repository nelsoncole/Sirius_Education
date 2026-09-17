/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: ramfs.c
 *    Description: Implementação integral do Sistema de Ficheiros em RAM (RamFS).
 *                 Suporta criação, leitura, escrita e enumeração em memória.
 * 
 *         Author: Nelson Cole
 *   Created Date: 16/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 16/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */
#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>

/* Estrutura interna para listar as entradas de um diretório em RAM */
typedef struct ramfs_entry {
    vfs_node_t*         node;
    struct ramfs_entry* next;
} ramfs_entry_t;

/* Estrutura interna para gerir o buffer dinâmico de dados de um ficheiro em RAM */
typedef struct ramfs_file_data {
    uint8_t* buffer;      // Bloco de memória RAM contendo os bytes do ficheiro
    uint32_t buffer_cap;  // Capacidade total alocada na RAM (em bytes)
} ramfs_file_data_t;

/* Protótipos das funções polimórficas do RamFS */
static int         ramfs_open(vfs_node_t* node, uint32_t flags);
static int         ramfs_close(vfs_node_t* node);
static int         ramfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);
static int         ramfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);
static int         ramfs_flush(vfs_node_t* node);
static vfs_node_t* ramfs_finddir(vfs_node_t* parent, const char* name);
static int         ramfs_readdir(vfs_node_t* target, uint32_t index, vfs_node_t* out_node);
static int         ramfs_mkdir(vfs_node_t* parent, const char* name, uint16_t permissions);
static int         ramfs_create(vfs_node_t* parent, const char* name, uint16_t permissions);
static int         ramfs_unlink(vfs_node_t* parent, const char* name);
static int         ramfs_rmdir(vfs_node_t* parent, const char* name);
static int         ramfs_stat(vfs_node_t* node, vfs_stat_t* buf);
static int         ramfs_chmod(vfs_node_t* node, uint16_t mode);
static int         ramfs_rename(vfs_node_t* parent, const char* old_name, const char* new_name);

/* Trancas Globais do Core do VFS para segurança de listas SMP */
extern void vfs_lock_tables(void);
extern void vfs_unlock_tables(void);

/* Tabela de Operações Completa do RamFS */
vfs_operations_t g_ramfs_ops = {
    .open    = ramfs_open,
    .close   = ramfs_close,
    .read    = ramfs_read,
    .write   = ramfs_write,
    .flush   = ramfs_flush,
    .finddir = ramfs_finddir,
    .readdir = ramfs_readdir,
    .mkdir   = ramfs_mkdir,
    .create  = ramfs_create,
    .unlink  = ramfs_unlink,
    .rmdir   = ramfs_rmdir,
    .stat    = ramfs_stat,
    .chmod   = ramfs_chmod,
    .rename  = ramfs_rename
};

//-----------------------------------------------------------------------------
// OPERAÇÕES DE CICLO DE VIDA E ES (FICHEIROS REGOULARES)
//-----------------------------------------------------------------------------
static int ramfs_open(vfs_node_t* node, uint32_t flags) {
    (void)flags;
    if (!node) return -1;
    return 0; // Ficheiros em RAM estão sempre prontos para acesso síncrono
}

/**
 * @brief Operação de fecho do driver de RAM.
 *        Liberta a memória RAM do nó apenas se ele for um nó temporário de sessão.
 */
static int ramfs_close(vfs_node_t* node) {
    if (!node) return -1;

    // IMPORTANTE: Só podemos dar kfree se o nó NÃO for a raiz global permanente
    // e se não for um nó persistente da árvore ativa do RamFS.
    // Para obter a raiz de forma limpa, usamos o getter que criámos.
    vfs_node_t* root = vfs_get_root();

    if (node != root && node != vfs_resolve_mountpoint(root)) {
        // Se o nó possui dados privados que foram alocados apenas para a sessão, limpa aqui:
        // (Nota: Só limpe o private_data aqui se ele foi clonado no open. 
        // Se for o private_data original do ficheiro, não faça kfree aqui, senão apaga o ficheiro!)
        
        kfree(node); // Liberta a estrutura temporária da RAM de forma síncrona
    }

    return 0; // Devolve 'int' (0 = Sucesso) para bater com a tabela g_ramfs_ops
}

static int ramfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    if (!node || !buffer || (node->flags & VFS_DIRECTORY)) return -1;

    ramfs_file_data_t* data = (ramfs_file_data_t*)node->private_data;
    if (!data || !data->buffer || offset >= node->size) return 0; // Fim do ficheiro

    // Ajusta o tamanho se o pedido ultrapassar o limite real do ficheiro
    if (offset + size > node->size) {
        size = (uint32_t)(node->size - offset);
    }

    memcpy(buffer, data->buffer + offset, size);
    return (int)size;
}

static int ramfs_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    if (!node || !buffer || (node->flags & VFS_DIRECTORY)) return -1;

    ramfs_file_data_t* data = (ramfs_file_data_t*)node->private_data;
    if (!data) return -1;

    // Redimensionamento Dinâmico do Buffer (Efeito memória elástica)
    if (offset + size > data->buffer_cap) {
        uint32_t new_cap = (uint32_t)(offset + size + 512); // Aloca com folga de 512 bytes
        uint8_t* new_buf = (uint8_t*)kmalloc(new_cap);
        if (!new_buf) return -2; // Out of memory

        memset(new_buf, 0, new_cap);
        if (data->buffer) {
            memcpy(new_buf, data->buffer, node->size);
            kfree(data->buffer);
        }
        data->buffer = new_buf;
        data->buffer_cap = new_cap;
    }

    memcpy(data->buffer + offset, buffer, size);
    
    // Atualiza o tamanho do ficheiro se ele cresceu
    if (offset + size > node->size) {
        node->size = offset + size;
    }

    return (int)size;
}

static int ramfs_flush(vfs_node_t* node) {
    (void)node;
    return 0; // RAM não possui caches físicas pendentes de escrita em disco
}

//-----------------------------------------------------------------------------
// OPERAÇÕES DE DIRETÓRIOS E COMPONENTES STRUCTURAIS
//-----------------------------------------------------------------------------
static vfs_node_t* ramfs_finddir(vfs_node_t* parent, const char* name) {
    if (!parent || !name || !(parent->flags & VFS_DIRECTORY)) return NULL;
    
    ramfs_entry_t* entry = (ramfs_entry_t*)parent->private_data;
    while (entry != NULL) {
        if (entry->node && strcmp(entry->node->name, name) == 0) {
            return entry->node;
        }
        entry = entry->next;
    }
    return NULL;
}

static int ramfs_readdir(vfs_node_t* target, uint32_t index, vfs_node_t* out_node) {
    if (!target || !out_node || !(target->flags & VFS_DIRECTORY)) return -1;

    ramfs_entry_t* entry = (ramfs_entry_t*)target->private_data;
    uint32_t current_idx = 0;

    while (entry != NULL) {
        if (current_idx == index) {
            memcpy(out_node, entry->node, sizeof(vfs_node_t));
            return 0;
        }
        current_idx++;
        entry = entry->next;
    }
    return -1;
}

/* Função interna auxiliar comum para criar objetos (Diretórios ou Ficheiros) */
static int ramfs_create_object(vfs_node_t* parent, const char* name, uint16_t permissions, uint32_t flags) {
    if (!parent || !name || name[0] == '\0') return -1;
    if (ramfs_finddir(parent, name) != NULL) return -2; // Já existe

    vfs_node_t* new_node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!new_node) return -3;

    memset(new_node, 0, sizeof(vfs_node_t));
    
    // CORREÇÃO DE SEGURANÇA: Garante o encerramento da string em C
    strncpy(new_node->name, name, sizeof(new_node->name) - 1);
    new_node->name[sizeof(new_node->name) - 1] = '\0';
    
    new_node->flags       = flags;
    new_node->size        = 0;
    new_node->inode       = (uint64_t)new_node; 
    new_node->permissions = permissions;
    new_node->ops         = &g_ramfs_ops;
    new_node->fs          = parent->fs;

    if (flags == VFS_FILE) {
        ramfs_file_data_t* fdata = (ramfs_file_data_t*)kmalloc(sizeof(ramfs_file_data_t));
        if (!fdata) { kfree(new_node); return -3; }
        fdata->buffer = NULL;
        fdata->buffer_cap = 0;
        new_node->private_data = fdata;
    } else {
        new_node->private_data = NULL; 
    }

    ramfs_entry_t* new_entry = (ramfs_entry_t*)kmalloc(sizeof(ramfs_entry_t));
    if (!new_entry) {
        if (flags == VFS_FILE) kfree(new_node->private_data);
        kfree(new_node);
        return -3;
    }
    new_entry->node = new_node;

    vfs_lock_tables();
    new_entry->next = (ramfs_entry_t*)parent->private_data;
    parent->private_data = new_entry;
    vfs_unlock_tables();

    return 0;
}

static int ramfs_mkdir(vfs_node_t* parent, const char* name, uint16_t permissions) {
    return ramfs_create_object(parent, name, permissions, VFS_DIRECTORY);
}

static int ramfs_create(vfs_node_t* parent, const char* name, uint16_t permissions) {
    return ramfs_create_object(parent, name, permissions, VFS_FILE);
}

//-----------------------------------------------------------------------------
// OPERAÇÕES DE REMOÇÃO E METADADOS
//-----------------------------------------------------------------------------
static int ramfs_remove_object(vfs_node_t* parent, const char* name, uint32_t expected_flag) {
    if (!parent || !name) return -1;

    vfs_lock_tables();
    ramfs_entry_t* prev = NULL;
    ramfs_entry_t* curr = (ramfs_entry_t*)parent->private_data;

    while (curr != NULL) {
        if (curr->node && strcmp(curr->node->name, name) == 0) {
            if (curr->node->flags != expected_flag) {
                vfs_unlock_tables();
                return -2; // Incompatibilidade de tipo (ex: rmdir num ficheiro)
            }
            
            // Se for diretório, impede remoção se ele contiver ficheiros lá dentro
            if (expected_flag == VFS_DIRECTORY && curr->node->private_data != NULL) {
                vfs_unlock_tables();
                return -3; // Diretório não está vazio
            }

            // Remove o elo da lista encadeada
            if (prev) prev->next = curr->next;
            else parent->private_data = curr->next;

            vfs_unlock_tables();

            // Liberta a memória física associada do Kernel heap
            if (expected_flag == VFS_FILE) {
                ramfs_file_data_t* fdata = (ramfs_file_data_t*)curr->node->private_data;
                if (fdata) {
                    if (fdata->buffer) kfree(fdata->buffer);
                    kfree(fdata);
                }
            }
            kfree(curr->node);
            kfree(curr);
            return 0; // Removido com sucesso
        }
        prev = curr;
        curr = curr->next;
    }

    vfs_unlock_tables();
    return -4; // Não encontrado
}

static int ramfs_unlink(vfs_node_t* parent, const char* name) {
    return ramfs_remove_object(parent, name, VFS_FILE);
}

static int ramfs_rmdir(vfs_node_t* parent, const char* name) {
    return ramfs_remove_object(parent, name, VFS_DIRECTORY);
}

static int ramfs_stat(vfs_node_t *node, vfs_stat_t *buf)
{
    if (!node || !buf)
        return -1;

    buf->st_ino  = (uint32_t)node->inode;
    buf->st_size = node->size;
    
    // POSIX Standard: O st_mode combina o tipo (flags) com as permissões lógicas
    buf->st_mode = node->flags | node->permissions;
    
    // Valores padrão de segurança (root/root)
    buf->st_uid  = 0;
    buf->st_gid  = 0;
    
    // RamFS não possui atributos nativos de disco como o FAT32 (Hidden, System)
    buf->st_attr = 0; 

    return 0;
}

static int ramfs_chmod(vfs_node_t *node, uint16_t mode)
{
    if (!node)
        return -1;
    node->permissions = mode;
    return 0;
}
static int ramfs_rename(vfs_node_t *parent, const char *old_name, const char *new_name)
{
    if (!parent || !old_name || !new_name)
        return -1;
    vfs_node_t *target = ramfs_finddir(parent, old_name);
    if (!target)
        return -2;
    if (ramfs_finddir(parent, new_name) != NULL)
        return -3; // Destino já existe
    vfs_lock_tables();
    strncpy(target->name, new_name, sizeof(target->name) - 1);
    target->name[sizeof(target->name) - 1] = '\0';
    vfs_unlock_tables();
    return 0;
}