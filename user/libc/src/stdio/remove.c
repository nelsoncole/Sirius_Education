/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: remove.c
 *    Description: Implementação da função padrão remove() da libc.
 *                 Mapeia a remoção diretamente para a chamada de sistema
 *                 unlink() exposta pelo VFS do Kernel.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 25/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <unistd.h>

/**
 * remove - Remove um ficheiro do sistema de ficheiros.
 * @path: Caminho completo ou relativo do ficheiro a apagar.
 * @return: 0 em caso de sucesso, ou -1 em caso de erro.
 */
int remove(const char *path)
{
    // 1. Validação de sanidade do ponteiro
    if (path == NULL || path[0] == '\0') 
    {
        return -1;
    }

    // 2. Invoca a chamada de sistema unlink() que implementada em vfs_syscalls.c
    // O kernel através da SYS_UNLINK tratará de libertar os inodes/blocos.
    int result = unlink(path);

    return result;
}