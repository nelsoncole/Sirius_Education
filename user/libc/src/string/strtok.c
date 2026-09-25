#include <string.h>
#include <stddef.h>

char *strtok_r(char *s, const char *delim, char **last) {
    char *tok;
    if (s == NULL) {
        s = *last;
        if (!s) return NULL;
    }

    // Pula delimitadores iniciais
    while (*s && strchr(delim, *s)) s++;
    if (!*s) {
        *last = NULL;
        return NULL;
    }

    tok = s;

    // Procura próximo delimitador
    while (*s) {
        if (strchr(delim, *s)) {
            *s = '\0';
            s++;
            break;
        }
        s++;
    }

    *last = s;
    return tok;
}

char *strtok(char *restrict s1, const char *restrict s2) {
    static char *last;
    return strtok_r(s1, s2, &last);
}