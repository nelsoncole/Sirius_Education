#include <string.h>
#include <stdlib.h>

char *strdup(const char *s) {
    if (!s) return NULL;

    size_t len = strlen(s) + 1;          // inclui o '\0'
    char *copy = (char *)malloc(len);    // aloca memória

    if (!copy) return NULL;               // falha na alocação

    memcpy(copy, s, len);                // copia toda a string incluindo '\0'

    return copy;
}