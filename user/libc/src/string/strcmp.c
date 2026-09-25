#include <string.h>

int strcmp (const char* s1, const char* s2)
{
	while (*s1 && (*s1 == *s2))  // enquanto caracteres iguais e não acabou
    {
        s1++;
        s2++;
    }

    // retorna diferença entre os dois caracteres
    return (unsigned char)*s1 - (unsigned char)*s2;
}
