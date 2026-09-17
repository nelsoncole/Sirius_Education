/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_tty.h
 *    Description: Cabeçalho de integração do subsistema TTY ao VFS Core.
 *                 Expõe a interface polimórfica de dispositivo de caracteres.
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

#ifndef _VFS_TTY_H_
#define _VFS_TTY_H_

#include <kernel/fs/vfs/vfs.h>

/**
 * tty_vfs_init - Inicializa e configura o nó estático da TTY no VFS.
 *                Associa as tabelas de operações polimórficas (vfs_operations_t)
 *                e vincula a instância lógica da TTY.
 *                Deve ser chamado uma única vez durante o boot do Kernel.
 */
void tty_vfs_init(void);

/**
 * tty_vfs_get_node - Retorna o ponteiro global do nó da TTY (VFS_CHAR_DEV).
 *                   Permite que o Kernel e o gestor de processos mapeiem 
 *                   os descritores de ficheiro padrão (stdin, stdout, stderr).
 * 
 * @return Ponteiro para a estrutura vfs_node_t estável da TTY.
 */
vfs_node_t* tty_vfs_get_node(void);

#endif /* _VFS_TTY_H_ */