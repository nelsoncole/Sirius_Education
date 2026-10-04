/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: mkdir.c
 *    Description: Utilitário para criação de novas diretorias no VFS do sistema.
 *                 Resolve caminhos relativos em espaço de utilizador e garante
 *                 o envio de caminhos absolutos limpos para o Kernel.
 * 
 *         Author: Nelson Cole
 *   Created Date: 04/10/2026
 *        License: MIT
 * ============================================================================
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/usyscall.h>

#ifndef MAX_PATH_LEN
#define MAX_PATH_LEN 4096
#endif

int main(int argc, char *argv[])
{
    // 1. Verificação de argumentos (Padrão POSIX mkdir)
    if (argc < 2) {
        fprintf(stderr, "mkdir: falta o operando de diretoria\n");
        fprintf(stderr, "Utilizacao: %s <nome_da_diretoria1> [nome_da_diretoria2 ...]\n", "mkdir");
        _exit(1);
    }

    char pwd[MAX_PATH_LEN];
    int pwd_carregado = 0;

    // 2. Processa cada um dos argumentos fornecidos
    for (int i = 1; i < argc; i++) 
    {
        const char *target_dir = argv[i];
        char absolute_path[MAX_PATH_LEN];
        memset(absolute_path, 0, MAX_PATH_LEN);

        /* --- RESOLUÇÃO DO CAMINHO ABSOLUTO --- */
        if (target_dir[0] == '/')
        {
            // Se já começa com '/', é um caminho absoluto completo
            strncpy(absolute_path, target_dir, MAX_PATH_LEN - 1);
        }
        else
        {
            // Se for relativo (ex: "pasta1"), junta com o PWD atual do processo
            if (!pwd_carregado)
            {
                if (getcwd(pwd, MAX_PATH_LEN) == NULL)
                {
                    fprintf(stderr, "mkdir: erro ao obter a diretoria atual (getcwd)\n");
                    _exit(1);
                }
                pwd_carregado = 1; // Reutiliza o buffer nas próximas iterações
            }
            
            snprintf(absolute_path, MAX_PATH_LEN, "%s/%s", pwd, target_dir);
        }
        /* -------------------------------------- */

        // Define as permissões padrão 0755 com base no seu ficheiro .h atualizado
        mode_t mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

        // 3. Invoca a Syscall passando o caminho absoluto resolvido
        if ((int)syscall2(SYS_MKDIR, (uint64_t)absolute_path, mode) < 0) 
        {
            fprintf(stderr, "mkdir: nao foi possivel criar a diretoria '%s': Erro no VFS\n", target_dir);
            _exit(1);
        }
        else 
        {
            printf("mkdir: Diretoria '%s' criada com sucesso.\n", target_dir);
        }
    }

    _exit(0);
    return 0;
}
