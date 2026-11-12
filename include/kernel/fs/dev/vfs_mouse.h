/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_mouse.h
 *    Description: Cabeçalho da interface VFS do dispositivo de rato.
 *                 Declara as funções de inicialização e acesso ao nó
 *                 VFS do rato, permitindo a integração do controlador
 *                 PS/2 com o sistema de ficheiros virtual.
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
#ifndef _VFS_MOUSE_H_
#define _VFS_MOUSE_H_

#include <kernel/fs/vfs/vfs.h>

void mouse_vfs_init(vfs_node_t* dev_node);
vfs_node_t* mouse_vfs_get_node(void);

#endif