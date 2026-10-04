/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: rename.c
 *    Description: Utilitário para renomear ou mover ficheiros e diretorias 
 *                 no VFS, consumindo a função nativa da libc.
 * 
 *         Author: Nelson Cole
 *   Created Date: 04/10/2026
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    // 1. Validação estrita de argumentos POSIX
    if (argc != 3) {
        fprintf(stderr, "rename: operando incorreto\n");
        fprintf(stderr, "Utilizacao: %s <nome_antigo> <nome_novo>\n", argv[0]);
        exit(1);
    }

    const char *old_path = argv[1];
    const char *new_path = argv[2];

    // 2. Chamada à função da libc do Sirius OS
    // Ela invoca a respetiva Syscall internamente para alterar o nome no VFS/FAT
    if (rename(old_path, new_path) < 0) 
    {
        fprintf(stderr, "rename: nao foi possivel renomear '%s' para '%s'\n", old_path, new_path);
        exit(1);
    }

    printf("rename: '%s' alterado para '%s' com sucesso.\n", old_path, new_path);
    
    exit(0);
}