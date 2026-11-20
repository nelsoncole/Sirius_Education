/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_keyboard.c
 *    Description: Implementação da interface VFS para o dispositivo do teclado.
 *                 Regista o teclado PS/2 no sistema de ficheiros virtual,
 *                 disponibilizando acesso  directo aos eventos do teclado através de
 *                 /dev/event0 e integrando o controlador com o VFS.
 *
 *         Author: Nelson Cole
 *   Created Date: 18/11/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 18/11/2026
 *
 *        License: MIT
 * ============================================================================
 */
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/dev/vfs_keyboard.h>
#include <kernel/drivers/char/keyboard.h>
#include <kernel/klib.h>
#include <kernel/kernel/core/spinlock.h>

static int tfs_keyboard_open(vfs_node_t* node, uint32_t flags);
static int tfs_keyboard_close(vfs_node_t* node);
static int tfs_keyboard_read(vfs_node_t* node, uint64_t offset,
                          uint32_t size, void* buffer);
static int tfs_keyboard_ioctl(vfs_node_t* node, uint32_t cmd,
                           unsigned long arg);

static vfs_operations_t g_keyboard_vfs_ops = {
    .open    = tfs_keyboard_open,
    .close   = tfs_keyboard_close,
    .read    = tfs_keyboard_read,
    .write   = NULL,
    .ioctl   = tfs_keyboard_ioctl,
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

static vfs_node_t* g_keyboard_node = NULL;
static spinlock_t g_vfs_keyboard_lock;
static uint32_t g_keyboard_inode = 3000;

static int tfs_keyboard_open(vfs_node_t* node, uint32_t flags)
{
    (void)flags;

    if (!node || node->flags != VFS_CHAR_DEV)
        return -1;

    return 0;
}

static int tfs_keyboard_close(vfs_node_t* node)
{
    (void)node;
    return 0;
}

static int tfs_keyboard_read(vfs_node_t* node, uint64_t offset,
                          uint32_t size, void* buffer)
{
    (void)offset;

    if (!node || !buffer || size < sizeof(keyboard_packet_t))
        return -1;

    keyboard_packet_t packet;

    if (!keyboard_get_packet(&packet))
        return 0;

    memcpy(buffer, &packet, sizeof(keyboard_packet_t));

    //kprintf("%x %x %x\n", packet.scancode, packet.mods, packet.is_e0_extended);

    return sizeof(keyboard_packet_t);
}

static int tfs_keyboard_ioctl(vfs_node_t* node, uint32_t cmd,
                           unsigned long arg)
{
    (void)node;
    (void)cmd;
    (void)arg;

    return -1;
}

void keyboard_vfs_init(vfs_node_t* dev_node)
{
    if (!dev_node || !dev_node->ops ||
        !dev_node->ops->create || !dev_node->ops->finddir)
    {
        kprintf("[VFS KEYBOARD] ERRO: Diretorio /dev invalido.\n");
        return;
    }

    spin_lock_init(&g_vfs_keyboard_lock);

    dev_node->ops->create(dev_node, "event0", 0666);

    vfs_node_t* node = dev_node->ops->finddir(dev_node, "event0");

    if (!node)
    {
        kprintf("[VFS KEYBOARD] ERRO: Falha ao criar /dev/event0.\n");
        return;
    }

    spin_lock(&g_vfs_keyboard_lock);

    node->flags = VFS_CHAR_DEV;
    node->inode = g_keyboard_inode++;
    node->ops = &g_keyboard_vfs_ops;
    node->private_data = NULL;

    g_keyboard_node = node;

    spin_unlock(&g_vfs_keyboard_lock);

    kprintf("[VFS KEYBOARD] Dispositivo /dev/event0 registado.\n");
}

vfs_node_t* keyboard_vfs_get_node(void)
{
    return g_keyboard_node;
}
