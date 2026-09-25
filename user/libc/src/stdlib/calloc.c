#include <stdlib.h>
#include <sys/uheap.h>

void *calloc(size_t nmemb, size_t size) 
{
	return ucalloc(nmemb, size);
}
