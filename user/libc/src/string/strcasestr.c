#include <ctype.h>
#include <stddef.h>
#include <string.h>

char* strcasestr(const char* haystack, const char* needle)
{
    if (!haystack || !needle)
        return NULL;

    if (*needle == '\0')
        return (char*)haystack;

    unsigned char first = (unsigned char)tolower((unsigned char)*needle);
    needle++;

    size_t needle_len = 0;
    const char* tmp = needle;
    while (*tmp++) needle_len++;

    for (; *haystack; haystack++)
    {
        if (tolower((unsigned char)*haystack) != first)
            continue;

        const char* h = haystack + 1;
        const char* n = needle;
        size_t i = needle_len;

        while (i && *h && tolower((unsigned char)*h) == tolower((unsigned char)*n))
        {
            h++;
            n++;
            i--;
        }

        if (i == 0)
            return (char*)haystack;
    }

    return NULL;
}