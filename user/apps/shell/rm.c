/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: rm.c
 *    Description: Utilitário otimizado para remoção de ficheiros ou diretorias.
 *                 Passa os argumentos diretamente para unlink() e rmdir().
 * 
 *         Author: Nelson Cole
 *   Created Date: 06/10/2026
 *        License: MIT
 * ============================================================================
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[])
{
    int recursive = 0;
    int file_idx = 1;

    // 1. Verificação de argumentos mínimos (Padrão POSIX)
    if (argc < 2) {
        fprintf(stderr, "rm: falta o operando\n");
        fprintf(stderr, "Utilizacao: %s [-r] <alvo1> [alvo2 ...]\n", argv[0]);
        _exit(1);
    }

    // 2. Verifica se a flag de recursividade (-r ou -R) foi passada
    if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "-R") == 0) {
        recursive = 1;
        file_idx = 2;
        if (argc < 3) {
            fprintf(stderr, "rm: falta o operando após '%s'\n", argv[1]);
            _exit(1);
        }
    }

    // 3. Processa cada um dos alvos fornecidos diretamente
    for (int i = file_idx; i < argc; i++) 
    {
        // Tenta remover primeiro como ficheiro através de unlink()
        int ret = unlink(argv[i]);
        
        // Se falhou e a flag recursiva estiver ativa, tenta remover como diretoria através de rmdir()
        if (ret < 0 && recursive) {
            ret = rmdir(argv[i]);
        }

        if (ret < 0) {
            fprintf(stderr, "rm: nao foi possivel remover '%s': Ficheiro/Diretoria nao encontrada ou erro no VFS\n", argv[i]);
            _exit(1);
        } else {
            printf("rm: '%s' removido com sucesso.\n", argv[i]);
        }
    }

    _exit(0);
    return 0;
}