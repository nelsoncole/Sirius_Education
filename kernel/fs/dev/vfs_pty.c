/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_pty.c
 *    Description: Integração do subsistema PTY (Pseudo-Terminais) Dinâmicos
 *                 no padrão Unix98 (/dev/ptmx e /dev/pts/X) dentro do VFS.
 *                 O nó /dev/ptmx funciona como um multiplexador/fábrica que
 *                 aloca dinamicamente pares PTY sob procura.
 *                 Totalmente protegido por Spinlocks para conformidade SMP.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 22/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/fs/vfs/vfs.h>
#include <kernel/drivers/tty/tty.h>
#include <kernel/fs/dev/vfs_pty.h>
#include <kernel/fs/dev/vfs_tty.h>
#include <kernel/klib.h>

/* Protótipos das operações em conformidade estrita com vfs_operations_t */
static int tfs_ptmx_open(vfs_node_t* node, uint32_t flags);
static int tfs_pty_replica_open(vfs_node_t* node, uint32_t flags);
static int tfs_pty_close(vfs_node_t* node);
static int tfs_pty_master_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);
static int tfs_pty_master_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);
static int tfs_pty_replica_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);
static int tfs_pty_replica_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer);

/* Operações VFS para o nó Fábrica /dev/ptmx */
static vfs_operations_t g_ptmx_vfs_ops = {
    .open    = tfs_ptmx_open,
    .close   = tfs_pty_close,
    .read    = tfs_pty_master_read,
    .write   = tfs_pty_master_write,
    .flush   = NULL, .finddir = NULL, .readdir = NULL, .mkdir = NULL,
    .create  = NULL, .unlink  = NULL, .rmdir   = NULL, .stat  = NULL,
    .chmod   = NULL, .rename  = NULL
};

/* Operações VFS para as Réplicas em /dev/pts/X */
static vfs_operations_t g_pty_replica_ops = {
    .open    = tfs_pty_replica_open,
    .close   = tfs_pty_close,
    .read    = tfs_pty_replica_read,
    .write   = tfs_pty_replica_write,
    .flush   = NULL, .finddir = NULL, .readdir = NULL, .mkdir = NULL,
    .create  = NULL, .unlink  = NULL, .rmdir   = NULL, .stat  = NULL,
    .chmod   = NULL, .rename  = NULL
};

/* Variáveis de controlo global e sincronia do driver */
static vfs_node_t* g_pts_root_node = NULL; 
static spinlock_t  g_vfs_pty_lock = {0};
static uint32_t    g_pty_inode_counter = 2000;
static uint32_t    g_pty_id_counter    = 0;

/* Ponteiro atómico global que guarda a PTY que detém o foco do teclado ativo */
static struct tty_device* g_active_pty_engine = NULL;

/**
 * pty_vfs_get_active_engine - Devolve a estrutura da PTY ativa para a thread do teclado.
 */
struct tty_device* pty_vfs_get_active_engine(void) {
    struct tty_device* active;
    spin_lock(&g_vfs_pty_lock);
    active = g_active_pty_engine;
    spin_unlock(&g_vfs_pty_lock);
    return active;
}

/**
 * pty_vfs_set_active_engine - Define qual PTY assume o foco do teclado em Ring 0.
 */
void pty_vfs_set_active_engine(struct tty_device* pty) {
    spin_lock(&g_vfs_pty_lock);
    g_active_pty_engine = pty;
    spin_unlock(&g_vfs_pty_lock);
}

static struct tty_device* pty_create_tty_instance(void) {
    struct tty_device* tty = (struct tty_device*)kmalloc(sizeof(struct tty_device));
    if (!tty) return NULL;
    memset(tty, 0, sizeof(struct tty_device));

    tty->in_head = tty->in_tail = 0;
    tty->out_head = tty->out_tail = 0;
    tty->line_start = tty->raw_count = tty->lines_available = 0;
    tty->c_lflag = TTY_ICANON | TTY_ECHO;
    spin_lock_init(&tty->lock);
    
    return tty;
}

/**
 * vfs_pty_allocate_internal - Fábrica interna encapsulada. 
 *                              Cria as estruturas na RAM e o nó em /dev/pts/X.
 */
static pty_pair_t* vfs_pty_allocate_internal(void) {
    if (!g_pts_root_node) return NULL;

    pty_pair_t* pair = (pty_pair_t*)kmalloc(sizeof(pty_pair_t));
    if (!pair) return NULL;
    memset(pair, 0, sizeof(pty_pair_t));

    pair->replica_tty = pty_create_tty_instance();
    if (!pair->replica_tty) {
        kfree(pair);
        return NULL;
    }

    spin_lock(&g_vfs_pty_lock);
    pair->id = g_pty_id_counter++;
    spin_unlock(&g_vfs_pty_lock);

    char replica_name[16];
    ksprintf(replica_name, "%d", pair->id);

    g_pts_root_node->ops->create(g_pts_root_node, replica_name, 0620);
    vfs_node_t* r_node = g_pts_root_node->ops->finddir(g_pts_root_node, replica_name);

    if (!r_node) {
        kfree(pair->replica_tty);
        kfree(pair);
        return NULL;
    }

    spin_lock(&g_vfs_pty_lock);
    r_node->flags        = VFS_CHAR_DEV;
    r_node->inode        = g_pty_inode_counter++;
    r_node->ops          = &g_pty_replica_ops;
    r_node->private_data = pair;
    pair->replica_node   = r_node;
    spin_unlock(&g_vfs_pty_lock);

    pty_vfs_set_active_engine(pair->replica_tty);

    return pair;
}

static int tfs_ptmx_open(vfs_node_t* node, uint32_t flags) {
    (void)flags;
    if (!node) return -1;

    // Aloca recorrendo à fábrica atómica
    pty_pair_t* pair = vfs_pty_allocate_internal();
    if (!pair) return -1;

    spin_lock(&g_vfs_pty_lock);
    pair->master_open_count = 1;
    // IMPORTANTE: Associa o par dinâmico ao nó privado desta abertura de ficheiro específica
    node->private_data = pair;
    pair->master_node = node;
    spin_unlock(&g_vfs_pty_lock);

    pty_vfs_set_active_engine(pair->replica_tty);

    return 0;
}

static int tfs_pty_replica_open(vfs_node_t* node, uint32_t flags) {
    (void)flags;
    if (!node || !node->private_data) return -1;

    pty_pair_t* pair = (pty_pair_t*)node->private_data;
    spin_lock(&g_vfs_pty_lock);
    pair->replica_open_count++;
    spin_unlock(&g_vfs_pty_lock);
    return 0;
}

static int tfs_pty_close(vfs_node_t* node) {
    if (!node || !node->private_data) return -1;

    pty_pair_t* pair = (pty_pair_t*)node->private_data;
    int destrutivel = 0;

    spin_lock(&g_vfs_pty_lock);
    if (node->ops == &g_ptmx_vfs_ops) {
        pair->master_open_count--;
    } else if (node->ops == &g_pty_replica_ops) {
        pair->replica_open_count--;
    }

    if (pair->master_open_count <= 0 && pair->replica_open_count <= 0) {
        destrutivel = 1;
    }
    spin_unlock(&g_vfs_pty_lock);

    if (destrutivel) {
        kprintf("[VFS PTY] Sessao /dev/pts/%d terminada. Destruindo nos...\n", pair->id);
        
        if (g_pts_root_node && g_pts_root_node->ops && g_pts_root_node->ops->unlink) {
            char replica_name[16];
            ksprintf(replica_name, "%d", pair->id);
            g_pts_root_node->ops->unlink(g_pts_root_node, replica_name);
        }

        spin_lock(&g_vfs_pty_lock);
        if (g_active_pty_engine == pair->replica_tty)
        {
            g_active_pty_engine = NULL; // Devolve o teclado ao Kernel puro
        }
        spin_unlock(&g_vfs_pty_lock);

        if (pair->replica_tty) kfree(pair->replica_tty);
        kfree(pair);
        node->private_data = NULL;
    }

    return 0;
}

static int tfs_pty_master_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    (void)offset;
    if (!node || !buffer || size == 0) return 0;

    pty_pair_t* pair = (pty_pair_t*)node->private_data;
    if (!pair || !pair->replica_tty) return -1;

    return tty_read(pair->replica_tty, (char*)buffer, size);
}

static int tfs_pty_master_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    (void)offset;
    if (!node || !buffer || size == 0) return 0;

    pty_pair_t* pair = (pty_pair_t*)node->private_data;
    if (!pair || !pair->replica_tty) return -1;

    return tty_write(pair->replica_tty, (const char*)buffer, size);
}

static int tfs_pty_replica_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    (void)offset;
    if (!node || !buffer || size == 0) return 0;

    pty_pair_t* pair = (pty_pair_t*)node->private_data;
    if (!pair || !pair->replica_tty) return -1;

    return tty_read(pair->replica_tty, (char*)buffer, size);
}

/*
static int tfs_pty_replica_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    (void)offset;
    if (!node || !buffer || size == 0) return 0;

    pty_pair_t* pair = (pty_pair_t*)node->private_data;
    if (!pair || !pair->replica_tty) return -1;

    return tty_write(pair->replica_tty, (const char*)buffer, size);
}*/
/* A Replica ESCREVE (Ex: printf do Bash) direcionando síncronamente para a tty0 */
extern void tty_putc_backbuffer_X(struct tty_device *tty, char c);
static int tfs_pty_replica_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    (void)offset;
    if (!node || !buffer || size == 0) return 0;

    pty_pair_t* pair = (pty_pair_t*)node->private_data;
    if (!pair || !pair->replica_tty) return -1;

    // 1. Escreve no buffer interno regulamentar da PTY
    int bytes_escritos = tty_write(pair->replica_tty, (const char*)buffer, size);

    // 2. REGRA DO JOGO: Resolve a tty0 e bombeia síncronamente os dados nela
    vfs_node_t* tty0_node = tty_vfs_get_node_by_name("tty0");
    if (tty0_node && tty0_node->private_data) {
        struct tty_device* tty0_device = (struct tty_device*)tty0_node->private_data;
        const char* src = (const char*)buffer;
        //tty_write(tty0_device, (const char*)buffer, bytes_escritos);
        for(int i=0; i < bytes_escritos; i++ ){
            tty_putc_backbuffer_X(tty0_device, src[i]);
        }

        __sync_lock_test_and_set(&tty0_device->refresh_needed, 1);

    }

    return bytes_escritos;
}

/**
 * pty_vfs_init - Prepara a árvore /dev criando a fábrica ptmx e a pasta pts/
 * @dev_node: O ponteiro para o nó de diretório "/dev".
 */
void pty_vfs_init(vfs_node_t* dev_node) {

    g_active_pty_engine = NULL;

    if (!dev_node || !dev_node->ops || !dev_node->ops->mkdir || 
        !dev_node->ops->create || !dev_node->ops->finddir) {
        kprintf("[VFS PTY] ERRO GRAVE: /dev pai nao possui operacoes polimorficas completas.\n");
        return;
    }

    spin_lock_init(&g_vfs_pty_lock);

    // 1. Cria a diretoria /dev/pts/ que guardará as réplicas numéricas
    dev_node->ops->mkdir(dev_node, "pts", 0755);
    g_pts_root_node = dev_node->ops->finddir(dev_node, "pts");

    if (!g_pts_root_node) {
        kprintf("[VFS PTY] ERRO: Falha catastrófica ao criar /dev/pts.\n");
        return;
    }

    // 2. Cria o ficheiro multiplexador central /dev/ptmx
    dev_node->ops->create(dev_node, "ptmx", 0666);
    vfs_node_t* ptmx_node = dev_node->ops->finddir(dev_node, "ptmx");

    if (ptmx_node) {
        spin_lock(&g_vfs_pty_lock);
        ptmx_node->flags = VFS_CHAR_DEV;
        ptmx_node->inode = 1999; // Inode mestre fixo para a fábrica
        ptmx_node->ops   = &g_ptmx_vfs_ops;  // Vincula as operações de fábrica
        spin_unlock(&g_vfs_pty_lock);
        kprintf("[VFS PTY] Fábrica Unix98 '/dev/ptmx' e pasta '/dev/pts/' prontas [SMP READY].\n");
    }
}