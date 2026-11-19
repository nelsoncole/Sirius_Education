/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_keyboard.h
 *    Description: Cabeçalho da interface VFS do dispositivo de teclado.
 *                 Declara as funções de inicialização e acesso ao nó
 *                 VFS do teclado, permitindo a integração do controlador
 *                 PS/2 com o sistema de ficheiros virtual.
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
#ifndef _VFS_KEYBOARD_H_
#define _VFS_KEYBOARD_H_

#include <kernel/fs/vfs/vfs.h>

void keyboard_vfs_init(vfs_node_t* dev_node);
vfs_node_t* keyboard_vfs_get_node(void);

#endif