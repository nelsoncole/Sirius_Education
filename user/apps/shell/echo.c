/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: echo.c
 *    Description: Utilitário minimalista e otimizado para exibição de texto.
 *                 Escreve diretamente no STDOUT usando a função write().
 * 
 *         Author: Nelson Cole
 *   Created Date: 06/10/2026
 *        License: MIT
 * ============================================================================
 */

#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    // 1. Se não existirem argumentos, apenas imprime uma quebra de linha
    if (argc < 2) {
        write(STDOUT_FILENO, "\n", 1);
        _exit(0);
    }

    // 2. Varre e imprime todos os argumentos fornecidos diretamente
    for (int i = 1; i < argc; i++) {
        write(STDOUT_FILENO, argv[i], strlen(argv[i]));

        // Injeta um espaço de separação caso não seja o último parâmetro
        if (i < argc - 1) {
            write(STDOUT_FILENO, " ", 1);
        }
    }

    // 3. Garante a quebra de linha final (LF) exigida pelo padrão POSIX
    write(STDOUT_FILENO, "\n", 1);

    _exit(0);
    return 0;
}