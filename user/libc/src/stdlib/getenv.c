/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: getenv.c
 *    Description: Implementação regulamentar da função getenv para a LibC.
 *                 Suporta varredura atómica no vetor global 'environ' (POSIX).
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * ============================================================================
 */

#include <stdlib.h>
#include <string.h>

#undef getenv

// Ponteiro global oficial POSIX para a tabela de variáveis de ambiente do processo.
// O Kernel preenche isto na inicialização da pilha do Ring 3.
extern char **environ;

// O teu ponteiro de fallback local
extern char *pwd;

/**
 * getenv - Procura e retorna o valor de uma variável de ambiente.
 * @name: O nome da variável desejada (ex: "PWD", "PATH").
 * 
 * Retorna: Ponteiro para o valor na RAM, ou NULL se não for encontrada.
 */
char *getenv(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return NULL;
    }

    size_t name_len = strlen(name);

    // 1. ABORDAGEM OFICIAL (POSIX): Varre o vetor global 'environ' se ele estiver mapeado
    if (environ != NULL) {
        for (size_t i = 0; environ[i] != NULL; i++) {
            // Verifica se a string começa com o nome procurado seguido de '='
            if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
                // Retorna o ponteiro para a posição imediatamente após o '=' (o valor real)
                return &environ[i][name_len + 1];
            }
        }
    }

    // 2. FALLBACK SEGURO DO SIRIUSOS: Se 'environ' não existir, confronta o teu PWD local
    if (strcmp(name, "PWD") == 0) {
        return pwd; // Sem necessidade de cast forçado (char*)
    }

    return NULL; // Variável não encontrada na tabela de ambiente
}