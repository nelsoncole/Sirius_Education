#include <string.h>
#include <stddef.h>

char *strchr(const char *s, int c)
{
    // converte c para char, conforme padrão C
    char ch = (char)c;

    while (*s)
    {
        if (*s == ch)
            return (char*)s;  // retorna ponteiro para a primeira ocorrência
        s++;
    }

    // verificar se queremos encontrar o '\0'
    if (ch == '\0')
        return (char*)s;

    return NULL;
}