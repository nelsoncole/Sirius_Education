/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_mouse.c
 *    Description: Implementação da interface VFS para o dispositivo de rato.
 *                 Regista o rato PS/2 no sistema de ficheiros virtual,
 *                 disponibilizando acesso aos eventos do rato através de
 *                 /dev/mouse e integrando o controlador com o VFS.
 *
 *         Author: Nelson Cole
 *   Created Date: 06/11/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 06/11/2026
 *
 *        License: MIT
 * ============================================================================
 */
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/dev/vfs_mouse.h>
#include <kernel/drivers/input/mouse.h>
#include <kernel/klib.h>
#include <kernel/kernel/core/spinlock.h>

static int tfs_mouse_open(vfs_node_t* node, uint32_t flags);
static int tfs_mouse_close(vfs_node_t* node);
static int tfs_mouse_read(vfs_node_t* node, uint64_t offset,
                          uint32_t size, void* buffer);
static int tfs_mouse_ioctl(vfs_node_t* node, uint32_t cmd,
                           unsigned long arg);

static vfs_operations_t g_mouse_vfs_ops = {
    .open    = tfs_mouse_open,
    .close   = tfs_mouse_close,
    .read    = tfs_mouse_read,
    .write   = NULL,
    .ioctl   = tfs_mouse_ioctl,
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

static vfs_node_t* g_mouse_node = NULL;
static spinlock_t g_vfs_mouse_lock;
static uint32_t g_mouse_inode = 2000;

static int tfs_mouse_open(vfs_node_t* node, uint32_t flags)
{
    (void)flags;

    if (!node || node->flags != VFS_CHAR_DEV)
        return -1;

    return 0;
}

static int tfs_mouse_close(vfs_node_t* node)
{
    (void)node;
    return 0;
}

static int tfs_mouse_read(vfs_node_t* node, uint64_t offset,
                          uint32_t size, void* buffer)
{
    (void)offset;

    if (!node || !buffer || size < sizeof(mouse_packet_t))
        return -1;

    mouse_packet_t packet;

    if (!mouse_get_packet(&packet))
        return 0;

    memcpy(buffer, &packet, sizeof(mouse_packet_t));

    return sizeof(mouse_packet_t);
}

static int tfs_mouse_ioctl(vfs_node_t* node, uint32_t cmd,
                           unsigned long arg)
{
    (void)node;
    (void)cmd;
    (void)arg;

    return -1;
}

void mouse_vfs_init(vfs_node_t* dev_node)
{
    if (!dev_node || !dev_node->ops ||
        !dev_node->ops->create || !dev_node->ops->finddir)
    {
        kprintf("[VFS MOUSE] ERRO: Diretorio /dev invalido.\n");
        return;
    }

    spin_lock_init(&g_vfs_mouse_lock);

    dev_node->ops->create(dev_node, "mouse", 0666);

    vfs_node_t* node = dev_node->ops->finddir(dev_node, "mouse");

    if (!node)
    {
        kprintf("[VFS MOUSE] ERRO: Falha ao criar /dev/mouse.\n");
        return;
    }

    spin_lock(&g_vfs_mouse_lock);

    node->flags = VFS_CHAR_DEV;
    node->inode = g_mouse_inode++;
    node->ops = &g_mouse_vfs_ops;
    node->private_data = NULL;

    g_mouse_node = node;

    spin_unlock(&g_vfs_mouse_lock);

    kprintf("[VFS MOUSE] Dispositivo /dev/mouse registado.\n");
}

vfs_node_t* mouse_vfs_get_node(void)
{
    return g_mouse_node;
}
