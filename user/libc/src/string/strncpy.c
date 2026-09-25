#include <string.h>

char* strncpy(char *dest, const char *src, size_t count)
{
	size_t i = 0;

    // copia os caracteres da fonte
    for (; i < count && src[i] != '\0'; i++)
        dest[i] = src[i];

    // preenche com '\0' se count > strlen(src)
    for (; i < count; i++)
        dest[i] = '\0';

    return dest;
}
