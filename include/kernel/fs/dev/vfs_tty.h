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
 *  Modified Date: 22/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _VFS_TTY_H_
#define _VFS_TTY_H_

#include <kernel/fs/vfs/vfs.h>

/**
 * tty_vfs_init - Instancia os múltiplos terminais e vincula-os dentro do nó /dev.
 *                Associa as tabelas de operações polimórficas (vfs_operations_t)
 *                e vincula a instância lógica da TTY.
 *                Deve ser chamado uma única vez durante o boot do Kernel.
 * @dev_node: O ponteiro para o nó de diretório "/dev" criado na RAM.
 */
void tty_vfs_init(vfs_node_t* dev_node);

/**
 * Procura dinâmica do nó com base na string literal do nome.
 * @name: O nome limpo da TTY procurada (ex: "tty0", "tty1").
 * @return: O ponteiro para o vfs_node_t correspondente, ou NULL se não encontrado.
 */
vfs_node_t* tty_vfs_get_node_by_name(const char* name);

/**
 * Resolve e retorna dinamicamente o nó da TTY ativa.
 * Substitui o antigo ponteiro fixo global por um lookup seguro no array de estados.
 */
vfs_node_t* tty_vfs_get_active_node(void);

/**
 * Altera programaticamente o ID do console em foco (ex: Alt+F1..F6).
 */
void tty_vfs_set_active_index(uint32_t index);

#endif /* _VFS_TTY_H_ */