/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: string.c
 *    Description: Implementação das funções básicas de manipulação de memória 
 *                 e strings (memset, memcpy, strlen, etc.) de forma autónoma 
 *                 (freestanding) para uso exclusivo do kernel.
 * 
 *         Author: Nelson Cole
 *   Created Date: 27/08/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 27/08/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#include <kernel/string.h>

void *memset(void *s, char val, size_t count)
{
	size_t i;
    unsigned char *tmp = (unsigned char *)s;
    for( i =0; i < count; i++) 
        *tmp++ = val;
    
    return s;
	
}

void *memcpy(void * restrict s1, const void * restrict s2, size_t n)
{	
	size_t p    = n;
	char *p_dest = (char*)s1;
	char *p_src  = (char*)s2;

	while(p--)
	*p_dest++ = *p_src++;
	return s1;
}

char *strcpy(char *dest, const char *src)
{
    	char *p = (char*)dest;

	char *p_dest = (char*)dest;
	char *p_src  = (char*)src;

 
    	while (*p_src != '\0') *p_dest++ = *p_src++;
    
	
	*p_dest = '\0';

	return p;
}

int strncpy(char *dest, const char *src,size_t count)
{

	char *p_dest = (char*)dest;
	char *p_src  = (char*)src;

    size_t i;
	for(i =0;i < count;i++) *p_dest++ = *p_src++;

	*p_dest ='\0';

    	return i;
}

size_t strlen(const char *s)
{
	char *tmp = (char*)s;
	
	while(*tmp != '\0')tmp++;

	return (size_t)(tmp - s);
}

int strcmp (const char* s1, const char* s2)
{

	const char *_s1 = s1;
	const char *_s2 = s2;

    	while((*_s1++ == *_s2++))
    	if((*_s1 == *_s2) && (*_s1 + *_s2 == 0))
    	return 0;


    	return -1;
}

int memcmp(char *s1, char *s2, int n)
{
   int i;
   for(i=0;i<n;i++)
   {
      if(s1[i] != s2[i])
         return 1;
   }
   return 0;
}