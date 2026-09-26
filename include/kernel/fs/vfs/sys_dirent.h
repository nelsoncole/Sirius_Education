/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sys_dirent.h
 *    Description: Estrutura e chamada de sistema sys_getdents blindada contra -O2.
 * 
 *         Author: Nelson Cole
 *   Created Date: 25/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 25/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _SYS_DIRENT_H_
#define _SYS_DIRENT_H_

#include <kernel/lib/stdint.h>
#include <kernel/kernel/sched/process.h>

// Definições de erro padrão do teu Kernel
#define EBADF   -9   // Bad file descriptor
#define EINVAL  -22  // Invalid argument
#define EFAULT  -14  // Bad address (ponteiros inválidos)

// Máscaras e tipos para o campo d_type da struct sys_dirent
#define DT_UNKNOWN  0
#define DT_DIR      4
#define DT_REG      8

/**
 * struct sys_dirent - Estrutura oficial do sistema para retorno de diretórios ao Ring 3.
 * O atributo 'packed' é fundamental para sincronização de bytes brutos com o Ring 3.
 */
struct sys_dirent {
    uint64_t        d_ino;    // Número único do Inode (específico do FS)
    uint64_t        d_off;    // Próximo offset (índice seguinte na tabela do VFS)
    unsigned short  d_reclen; // Tamanho total desta estrutura nesta iteração (com padding)
    unsigned char   d_type;   // Tipo do nó (DT_DIR, DT_REG, etc.)
    char            d_name[]; // Nome do elemento terminado em '\0' (tamanho dinâmico)
} __attribute__((packed));

/**
 * sys_getdents - Lê as entradas de um diretório aberto para o espaço do utilizador.
 * @fd:     Descritor de ficheiro do diretório (Ring 3).
 * @dirp:   Buffer no espaço do utilizador que receberá as estruturas sys_dirent.
 * @count:  Tamanho máximo do buffer fornecido (em bytes).
 */
static inline uint64_t sys_getdents(int fd, struct sys_dirent *dirp, uint32_t count) {
    process_t* proc = get_current_process();
    if (!proc || fd < 0 || fd >= MAX_FILES_PER_PROCESS) {
        return (uint64_t)EBADF;
    }

    vfs_file_t* file = proc->file_descriptor_table[fd];
    if (!file || !file->node) {
        return (uint64_t)EBADF;
    }

    // Mantemos uma referência direta e constante ao nó pai original do processo
    vfs_node_t* const parent_dir_node = file->node;

    if ((parent_dir_node->flags & VFS_DIRECTORY) == 0) {
        return (uint64_t)EINVAL;
    }

    if (!dirp || count < sizeof(struct sys_dirent) + 4) {
        return (uint64_t)EFAULT;
    }

    kprintf("[SCI] sys_getdents: fd=%d, dirp=0x%lx, count=%u, current_offset=%lu\n", 
            fd, (uint64_t)dirp, count, file->offset);

    uint8_t* user_buf = (uint8_t*)dirp;
    uint32_t bytes_written = 0;
    
    // Aloca dinamicamente no heap do Kernel (kmalloc) 
    // para impedir que a pilha do Ring 0 sofra Stack Overflow no ciclo do -O2.
    vfs_node_t* temp_node = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!temp_node) {
        return (uint64_t)-12; // -ENOMEM
    }

    // Estrutura intermédia local para evitar corrupção por Strict Aliasing no -O2
    struct sys_dirent local_dirent;

    while (1) {
        // Limpa completamente a memória do nó temporário
        memset(temp_node, 0, sizeof(vfs_node_t));

        // Invoca o readdir do driver através do nó constante guardado
        int res = -2;
        if (parent_dir_node->ops && parent_dir_node->ops->readdir) {
            res = parent_dir_node->ops->readdir(parent_dir_node, (uint32_t)file->offset, temp_node);
        }
        
        if (res < 0) {
            break; 
        }

        int name_len = strlen(temp_node->name);
        if (name_len == 0) {
            file->offset++;
            continue;
        }

        // Tamanho baseado na struct empacotada fixa (19 bytes base) + string + nulo
        int reclen = sizeof(struct sys_dirent) + name_len + 1;
        reclen = (reclen + 7) & ~7; // Alinhamento estrito a 8 bytes para o Ring 3

        if (bytes_written + reclen > count) {
            break;
        }

        // Preenche de forma estritamente isolada na pilha do Kernel
        memset(&local_dirent, 0, sizeof(struct sys_dirent));
        local_dirent.d_ino = (uint64_t)temp_node->inode; 
        local_dirent.d_reclen = (unsigned short)reclen;
        
        if (temp_node->flags & VFS_DIRECTORY) {
            local_dirent.d_type = DT_DIR;
        } else {
            local_dirent.d_type = DT_REG;
        }

        file->offset++;
        local_dirent.d_off = file->offset;

        // Transfere os bytes fixos do cabeçalho de forma opaca
        memcpy(user_buf + bytes_written, &local_dirent, sizeof(struct sys_dirent));

        // Transfere o nome com base no deslocamento numérico do offset físico
        uint32_t name_offset = __builtin_offsetof(struct sys_dirent, d_name);
        memcpy(user_buf + bytes_written + name_offset, temp_node->name, name_len);
        user_buf[bytes_written + name_offset + name_len] = '\0';

        bytes_written += reclen;
    }

    // Liberta com segurança o nó temporário do Heap
    kfree(temp_node);

    return (uint64_t)bytes_written;
}

#endif /* _SYS_DIRENT_H_ */