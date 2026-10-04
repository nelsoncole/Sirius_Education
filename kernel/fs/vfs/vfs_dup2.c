/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: vfs_dup2.c
 *    Description: Implementação das rotinas de duplicação de descritores de
 *                 ficheiros (dup2) e inicialização dos canais padrão de
 *                 E/S (stdin, stdout, stderr) para os processos.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 22/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 23/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */
#include <kernel/kernel/sched/process.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/klib.h>

/**
 * k_dup2 - Função interna do Kernel para duplicar descritores num processo específico.
 *          Garante o polimorfismo e a consistência de ref_count em ambiente SMP.
 */
int k_dup2(process_t* proc, int oldfd, int newfd) {
    if (!proc) return -1;

    // 1. Validação de limites nos descritores
    if (oldfd < 0 || oldfd >= MAX_FILES_PER_PROCESS) return -1;
    if (newfd < 0 || newfd >= MAX_FILES_PER_PROCESS) return -1;

    // 2. Se os FDs forem iguais e válidos, não faz nada
    if (oldfd == newfd) {
        return (proc->file_descriptor_table[oldfd]) ? newfd : -1;
    }

    // 3. O descritor de origem tem de apontar para um ficheiro/dispositivo válido
    vfs_file_t* old_file = proc->file_descriptor_table[oldfd];
    if (!old_file) return -1;

    // 4. Se o destino já tiver um ficheiro aberto, FECHA-O CORRETAMENTE
    vfs_file_t* file_to_close = proc->file_descriptor_table[newfd];
    if (file_to_close != NULL) {
        
        // Remove a referência do processo a este ficheiro antigo
        file_to_close->ref_count--;

        // Se mais nenhum processo (ou FD) estiver a usar este ficheiro antigo, liberta-o
        if (file_to_close->ref_count == 0) {
            if (file_to_close->node) {
                vfs_close(file_to_close->node); // Aciona o socket_vfs_close se for um socket!
            }
            kfree(file_to_close);
            kprintf("[VFS] k_dup2: Ficheiro anterior em FD %d fechado e libertado.\n", newfd);
        } else {
            kprintf("[VFS] k_dup2: Referencia do ficheiro em FD %d decrementada (Restam %u).\n", 
                    newfd, file_to_close->ref_count);
        }

        // Invalida o slot antes de receber o novo ficheiro
        proc->file_descriptor_table[newfd] = NULL;
    }

    // 5. CLONAGEM: O novo slot aponta para a mesma estrutura vfs_file_t
    proc->file_descriptor_table[newfd] = old_file;

    // 6. Incrementa o contador de referências da estrutura vfs_file_t
    old_file->ref_count++; 

    return newfd;
}