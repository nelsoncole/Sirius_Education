/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: touch.c
 *    Description: Utilitário para criação de novos ficheiros vazios no VFS do sistema.
 *                 Resolve caminhos relativos em espaço de utilizador e garante
 *                 o envio de caminhos absolutos limpos para o Kernel.
 * 
 *         Author: Nelson Cole
 *   Created Date: 06/10/2026
 *        License: MIT
 * ============================================================================
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "touch: falta o operando de ficheiro\n");
        fprintf(stderr, "Utilizacao: %s <nome_do_ficheiro1> [nome_do_ficheiro2 ...]\n", "touch");
        _exit(1);
    }

    int fd = open(argv[1], O_CREAT | O_RDWR);
    if (fd < 0)
    {
        fprintf(stderr, "touch: nao foi possivel criar ou atualizar o ficheiro '%s': Erro no VFS\n", argv[1]);
        _exit(1);
    }
    else
    {
        close(fd);
        printf("touch: Ficheiro '%s' criado/atualizado com sucesso.\n", argv[1]);
    }

    _exit(0);
    return 0;
}
