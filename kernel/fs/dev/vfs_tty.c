/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_tty.c
 *    Description: Integração do subsistema TTY como um nó de dispositivo de
 *                 caracteres (VFS_CHAR_DEV) dentro do VFS Core.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 15/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 15/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/fs/vfs/vfs.h>
#include <kernel/drivers/tty/tty.h>
#include <kernel/klib.h>

/* Protótipos das operações em conformidade estrita com vfs_operations_t */
static int tfs_tty_open(vfs_node_t* node, uint32_t flags);
static int tfs_tty_close(vfs_node_t* node);
static int tfs_tty_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);
static int tfs_tty_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);

/* Tabela polimórfica de operações para o dispositivo de caracteres */
static vfs_operations_t g_tty_vfs_ops = {
    .open    = tfs_tty_open,
    .close   = tfs_tty_close,
    .read    = tfs_tty_read,
    .write   = tfs_tty_write,
    .flush   = NULL,
    .finddir = NULL,
    .readdir = NULL,
    .mkdir   = NULL,
    .create  = NULL,
    .unlink  = NULL,
    .rmdir   = NULL,
    .stat    = NULL,
    .chmod   = NULL,
    .rename  = NULL
};

/* Ponteiro global que apontará para a TTY padrão do sistema (Console ativo) */
static vfs_node_t* g_main_tty_node = NULL;
static uint32_t g_tty_inode_counter = 1000;

static int tfs_tty_open(vfs_node_t* node, uint32_t flags) {
    (void)flags;
    if (!node) return -1;

    /* Vincula o ponteiro opaco private_data à estrutura ativa do TTY */
    node->private_data = tty_get_current();
    return 0;
}

static int tfs_tty_close(vfs_node_t* node) {
    (void)node;
    return 0;
}

static int tfs_tty_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    (void)offset; /* Ignorado em fluxos de caracteres contínuos */
    if (!node || !buffer || size == 0) return 0;

    struct tty_device* tty = (struct tty_device*)node->private_data;
    if (!tty) return -1;

    /* Consome do input_buf de forma segura para SMP */
    return tty_read(tty, (char*)buffer, size);
}

static int tfs_tty_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    (void)offset; /* Ignorado */
    if (!node || !buffer || size == 0) return 0;

    struct tty_device* tty = (struct tty_device*)node->private_data;
    if (!tty) return -1;

    /* Alimenta o output_buf assíncrono do TTY */
    return tty_write(tty, (const char*)buffer, size);
}

/**
 * tty_vfs_create_device - Fábrica Dinâmica de Terminais.
 *                         Aloca uma TTY isolada e o seu respetivo nó no VFS.
 */
vfs_node_t* tty_vfs_create_device(const char* name) {
    // 1. Aloca dinamicamente o nó do VFS para o dispositivo
    vfs_node_t* node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!node) return NULL;
    memset(node, 0, sizeof(vfs_node_t));

    // 2. Aloca dinamicamente a estrutura interna de buffers do TTY (O Motor)
    struct tty_device* tty = (struct tty_device*)kmalloc(sizeof(struct tty_device));
    if (!tty) {
        kfree(node);
        return NULL;
    }
    memset(tty, 0, sizeof(struct tty_device));

    // 3. Inicializa os componentes internos da nova TTY alocada
    tty->in_head = tty->in_tail = 0;
    tty->out_head = tty->out_tail = 0;
    tty->line_start = tty->raw_count = tty->lines_available = 0;
    tty->c_lflag = TTY_ICANON | TTY_ECHO;
    
    // Inicializa o spinlock global (que criámos em kernel/core/spinlock.c)
    spin_lock_init(&tty->lock);

    // 4. Preenche as propriedades do nó do VFS
    strncpy(node->name, name, VFS_NAME_MAX - 1);
    node->flags = VFS_CHAR_DEV;
    node->size = 0;
    node->inode = g_tty_inode_counter++; // Garante um Inode exclusivo por TTY
    node->permissions = 0666;
    node->ops = &g_tty_vfs_ops;
    
    /* VINCULAÇÃO DINÂMICA: Este nó passa a mandar exclusivamente nesta TTY */
    node->private_data = tty; 
    node->fs = NULL;
    node->ptr_mount = NULL;

    return node;
}

/**
 * tty_vfs_init - Instancia a primeira TTY padrão via kmalloc durante o boot.
 */
void tty_vfs_init(void) {
    /* Cria dinamicamente a TTY principal do sistema */
    g_main_tty_node = tty_vfs_create_device("tty0");
    
    if (g_main_tty_node) {
        kprintf("[VFS TTY] Terminal Dinamico '/dev/tty0' criado com sucesso.\n");
    }
}

vfs_node_t* tty_vfs_get_node(void) {
    return g_main_tty_node;
}