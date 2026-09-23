/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_tty.c
 *    Description: Integração do subsistema TTY como um nó de dispositivo de
 *                 caracteres (VFS_CHAR_DEV) dentro do VFS Core.
 *                 Usa a infraestrutura nativa do RamFS para criar múltiplos terminais.
 *                 Totalmente protegido por Spinlocks para conformidade SMP e
 *                 resolução de nós baseada em polimorfismo dinâmico.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 15/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 22/09/2026
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

/* A única tabela polimórfica para os dispositivos de caracteres TTY */
static vfs_operations_t g_tty_vfs_ops = {
    .open    = tfs_tty_open,
    .close   = tfs_tty_close,
    .read    = tfs_tty_read,
    .write   = tfs_tty_write,
    .flush   = NULL,
    .finddir = NULL, /* Nós de caracteres não contêm subdiretórios */
    .readdir = NULL,
    .mkdir   = NULL,
    .create  = NULL,
    .unlink  = NULL,
    .rmdir   = NULL,
    .stat    = NULL,
    .chmod   = NULL,
    .rename  = NULL
};

/* Array para gerir os nós das TTYs do sistema e o seu respetivo Lock de proteção */
static vfs_node_t* g_tty_devices[MAX_TTY_DEVICES] = {NULL};
static spinlock_t  g_vfs_tty_lock; // Lock estático para proteger as tabelas globais deste driver

/* Controlo e mapeamento dinâmico de Inodes e Terminal Ativo */
static uint32_t    g_active_tty_index = 0; // Armazena apenas o ID (0 a 5) do terminal ativo no ecrã
static uint32_t    g_tty_inode_counter = 1000;

static int tfs_tty_open(vfs_node_t* node, uint32_t flags) {
    (void)flags;
    if (!node) return -1;

    // Protege com o lock global do driver a verificação e escrita no nó
    spin_lock(&g_vfs_tty_lock);
    if (node->flags == VFS_CHAR_DEV && !node->private_data) {
        node->private_data = tty_get_current();
    }
    spin_unlock(&g_vfs_tty_lock);
    return 0;
}

static int tfs_tty_close(vfs_node_t* node) {
    (void)node;
    return 0;
}

static int tfs_tty_read(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    (void)offset; 
    if (!node || !buffer || size == 0) return 0;

    // Extração segura do ponteiro privado
    spin_lock(&g_vfs_tty_lock);
    struct tty_device* tty = (struct tty_device*)node->private_data;
    spin_unlock(&g_vfs_tty_lock);
    
    if (!tty) return -1;

    // Nota: O tty_read interno já deve possuir o seu próprio tty->lock (que inicializaste)
    return tty_read(tty, (char*)buffer, size);
}

static int tfs_tty_write(vfs_node_t* node, uint64_t offset, uint32_t size, void* buffer) {
    (void)offset; 
    if (!node || !buffer || size == 0) return 0;

    // Extração segura do ponteiro privado
    spin_lock(&g_vfs_tty_lock);
    struct tty_device* tty = (struct tty_device*)node->private_data;
    spin_unlock(&g_vfs_tty_lock);
    
    if (!tty) return -1;

    // Nota: O tty_write interno já deve possuir o seu próprio tty->lock
    return tty_write(tty, (const char*)buffer, size);
}

/**
 * tty_vfs_setup_nodestruct - Configura e converte um nó alocado pelo RamFS 
 *                            num nó de dispositivo de caracteres TTY válido.
 */
static int tty_vfs_setup_nodestruct(vfs_node_t* node) {
    if (!node) return -1;

    // 1. Aloca dinamicamente o motor de buffers do driver TTY
    struct tty_device* tty = (struct tty_device*)kmalloc(sizeof(struct tty_device));
    if (!tty) return -1;
    memset(tty, 0, sizeof(struct tty_device));

    // 2. Inicializa os componentes internos e o lock individual daquela TTY física
    tty->in_head = tty->in_tail = 0;
    tty->out_head = tty->out_tail = 0;
    tty->line_start = tty->raw_count = tty->lines_available = 0;
    tty->c_lflag = TTY_ICANON | TTY_ECHO;
    spin_lock_init(&tty->lock);

    // Vincula a instância ao catálogo global do driver físico usando a posição do loop (inode - 1000)
    tty_register_driver_instance(g_tty_inode_counter - 1000, tty);

    // 3. Substitui o buffer de arquivo padrão do RamFS pelo motor do TTY de forma segura
    spin_lock(&g_vfs_tty_lock);
    if (node->private_data) {
        kfree(node->private_data); 
    }

    // 4. Muda as propriedades do nó para se tornar um Dispositivo de Caracteres polimórfico
    node->flags        = VFS_CHAR_DEV;
    node->inode        = g_tty_inode_counter++;
    node->ops          = &g_tty_vfs_ops;
    node->private_data = tty;
    spin_unlock(&g_vfs_tty_lock);

    return 0;
}

/**
 * tty_vfs_init - Instancia os múltiplos terminais e vincula-os dentro do nó /dev.
 *                Garante a sincronia correta para inicialização SMP estável.
 * @dev_node: O ponteiro para o nó de diretório "/dev" criado na RAM.
 */
void tty_vfs_init(vfs_node_t* dev_node) {
    if (!dev_node || !dev_node->ops || !dev_node->ops->create || !dev_node->ops->finddir) {
        kprintf("[VFS TTY] ERRO: O no pai /dev nao suporta as operacoes polimorficas.\n");
        return;
    }

    // Inicializa o lock global deste driver antes de popular os arrays
    spin_lock_init(&g_vfs_tty_lock);

    kprintf("[VFS TTY] A popular o diretorio /dev com terminais virtuais via VFS...\n");

    char tty_name[8];
    for (int i = 0; i < MAX_TTY_DEVICES; i++) {
        tty_name[0] = 't'; 
        tty_name[1] = 't'; 
        tty_name[2] = 'y';
        tty_name[3] = '0' + i; 
        tty_name[4] = '\0';

        // 1. Cria o nó no RamFS usando a tabela do próprio pai
        dev_node->ops->create(dev_node, tty_name, 0666);
        
        // 2. Busca o nó recém-criado usando o polimorfismo estrito do pai (.finddir)
        vfs_node_t* target_node = dev_node->ops->finddir(dev_node, tty_name);
        
        if (target_node) {
            // Configura os buffers do TTY
            tty_vfs_setup_nodestruct(target_node);
            
            // Associa com segurança ao mapa estático indexado
            spin_lock(&g_vfs_tty_lock);
            g_tty_devices[i] = target_node;
            spin_unlock(&g_vfs_tty_lock);
            
            kprintf("[VFS TTY] Terminal '/dev/%s' injetado em seguranca [SMP READY].\n", tty_name);
        }
    }
}

/**
 * Procura dinâmica do nó com base na string literal do nome.
 * @name: O nome limpo da TTY procurada (ex: "tty0", "tty1").
 * @return: O ponteiro para o vfs_node_t correspondente, ou NULL se não encontrado.
 */
vfs_node_t* tty_vfs_get_node_by_name(const char* name) {
    if (!name) return NULL;

    spin_lock(&g_vfs_tty_lock);
    for (int i = 0; i < MAX_TTY_DEVICES; i++) {
        if (g_tty_devices[i] && strcmp(g_tty_devices[i]->name, name) == 0) {
            vfs_node_t* node = g_tty_devices[i];
            spin_unlock(&g_vfs_tty_lock);
            return node;
        }
    }
    spin_unlock(&g_vfs_tty_lock);
    return NULL;
}

/**
 * Resolve e retorna dinamicamente o nó da TTY ativa.
 * Substitui o antigo ponteiro fixo global por um lookup seguro no array de estados.
 */
vfs_node_t* tty_vfs_get_active_node(void) {
    spin_lock(&g_vfs_tty_lock);
    uint32_t active_idx = g_active_tty_index;
    vfs_node_t* node = (active_idx < MAX_TTY_DEVICES) ? g_tty_devices[active_idx] : NULL;
    spin_unlock(&g_vfs_tty_lock);
    return node;
}

/**
 * Altera programaticamente o ID do console em foco (ex: Alt+F1..F6).
 */
void tty_vfs_set_active_index(uint32_t index) {
    if (index >= MAX_TTY_DEVICES) return;

    spin_lock(&g_vfs_tty_lock);
    g_active_tty_index = index;
    spin_unlock(&g_vfs_tty_lock);
}