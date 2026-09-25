#include <string.h>

#include <string.h>

char *strcpy(char *dest, const char *src)
{
	if(!dest || !src )
		return NULL;
		
    char *start = dest;

    while (*src != '\0')
        *dest++ = *src++;

    *dest = '\0';

    return start;
}