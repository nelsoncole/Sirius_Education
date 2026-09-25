/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_pty.h
 *    Description: Protótipos e definições de controlo para a integração do
 *                 subsistema PTY (Pseudo-Terminais) Dinâmicos Unix98 no VFS.
 *                 Provê a interface pública para inicialização e gestão da
 *                 fábrica /dev/ptmx e das réplicas em /dev/pts/.
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

#ifndef _VFS_PTY_H_
#define _VFS_PTY_H_

#include <kernel/fs/vfs/vfs.h>
#include <kernel/drivers/tty/tty.h>

/* Estrutura de acoplamento dinâmico para ligar o par Master/Replica */
typedef struct pty_pair {
    int id;
    struct tty_device* replica_tty;  // Motor TTY partilhado para cruzamento de buffers
    vfs_node_t* master_node;         // O nó virtual do Master (vinculado ao FD do ptmx)
    vfs_node_t* replica_node;        // O nó físico em /dev/pts/X
    
    // Controlo de referências para libertação de memória segura [SMP SAFE]
    int master_open_count;
    int replica_open_count;
} pty_pair_t;


/**
 * @brief Inicializa o subsistema de Pseudo-Terminais e o lock mestre do driver.
 * 
 * Cria o diretório virtual `/dev/pts` na RAM e instancia o nó multiplexador
 * `/dev/ptmx` como um dispositivo de caracteres dinâmico. Deve ser invocado
 * durante a fase de montagem e população do DevFS/RamFS no boot do kernel.
 * 
 * @param dev_node O ponteiro para o nó de diretório pai "/dev" criado na RAM.
 */
void pty_vfs_init(vfs_node_t* dev_node);

struct tty_device* pty_vfs_get_active_engine(void);
void pty_vfs_set_active_engine(struct tty_device* pty);

#endif /* _VFS_PTY_H_ */
