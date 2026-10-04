#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Ponteiro global oficial POSIX para a tabela de ambiente
extern char **environ;

// Sinaliza se a tabela 'environ' atual foi alocada por nós (Libc) via heap,
// permitindo chamadas subsequentes de urealloc com segurança.
static int g_environ_is_allocated = 0;

/**
 * setenv - Adiciona ou modifica uma variável no ambiente do processo.
 * @name: Nome da variável (não pode conter '=').
 * @value: Valor a ser atribuído.
 * @overwrite: Se diferente de zero, substitui o valor caso a variável já exista.
 */
int setenv(const char *name, const char *value, int overwrite)
{
    if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL) {
        // POSIX especifica EINVAL para nomes inválidos ou que contenham '='
        return -1;
    }

    size_t name_len = strlen(name);
    size_t value_len = strlen(value);

    // 1. Monta a nova string no formato "NOME=VALOR"
    // +2 conta para o caractere '=' e o terminador '\0'
    size_t new_env_size = name_len + value_len + 2;
    char *new_env_string = (char *)malloc(new_env_size);
    if (new_env_string == NULL) {
        return -1; // Out of Memory
    }

    strcpy(new_env_string, name);
    strcat(new_env_string, "=");
    strcat(new_env_string, value);

    size_t count = 0;

    // 2. Verifica se a variável já existe na tabela corrente
    if (environ != NULL) {
        for (size_t i = 0; environ[i] != NULL; i++) {
            count++;
            if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
                if (!overwrite) {
                    // Não subscreve, liberta a string temporária e sai com sucesso
                    free(new_env_string);
                    return 0;
                }
                // Subscreve: Se o environ original veio da pilha do Kernel, não podemos dar free() nele.
                // Substituímos apenas o ponteiro pelo novo alocado no Heap do Ring 3
                environ[i] = new_env_string;
                return 0;
            }
        }
    }

    // 3. Se a variável não existia, precisamos expandir a tabela 'environ'
    // Aloca espaço para a tabela atual + nova entrada + NULL terminador
    size_t new_table_entries = count + 2; 
    char **new_environ = NULL;

    if (g_environ_is_allocated && environ != NULL) {
        // Se a tabela já reside no Heap, expandimo-la de forma limpa via realloc
        new_environ = (char **)realloc(environ, new_table_entries * sizeof(char *));
    } else {
        // Se é a primeira vez (ou herança do crt0), criamos uma tabela nova no Heap
        new_environ = (char **)malloc(new_table_entries * sizeof(char *));
        if (new_environ != NULL && environ != NULL) {
            // Copia cirurgicamente os ponteiros antigos da pilha para o Heap
            for (size_t i = 0; i < count; i++) {
                new_environ[i] = environ[i];
            }
        }
    }

    if (new_environ == NULL) {
        free(new_env_string);
        return -1; // Falha de alocação no Heap do utilizador
    }

    // 4. Insere a nova string na cauda e termina o vetor com NULL
    new_environ[count] = new_env_string;
    new_environ[count + 1] = NULL;

    // 5. Atualiza o ponteiro global oficial para que a Libc e a Shell o vejam
    environ = new_environ;
    g_environ_is_allocated = 1;

    return 0;
}