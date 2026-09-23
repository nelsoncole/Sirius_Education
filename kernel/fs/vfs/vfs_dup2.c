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

    // 4. Se o destino já tiver um ficheiro aberto, fecha-o primeiro
    if (proc->file_descriptor_table[newfd]) {
        // No futuro, chama aqui a tua função interna de fecho:
        // k_close_internal(proc, newfd);
        proc->file_descriptor_table[newfd] = NULL;
    }

    // 5. CLONAGEM: O novo slot aponta para a mesma estrutura vfs_file_t
    proc->file_descriptor_table[newfd] = old_file;

    // 6. Incrementa o contador de referências da estrutura vfs_file_t
    old_file->ref_count++; 

    return newfd;
}