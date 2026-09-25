#include <stdlib.h>
#include <string.h>

char *strndup(const char *s, size_t n)
{
    if (!s) return NULL;

    // calcular tamanho real (até n ou '\0')
    size_t len = 0;
    while (len < n && s[len] != '\0')
        len++;

    // alocar memória (+1 para '\0')
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;

    // copiar
    memcpy(out, s, len);
    out[len] = '\0';

    return out;
}