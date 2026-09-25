#include <stdlib.h>
#include <sys/uheap.h>

#undef        realloc

void *realloc(void *ptr, size_t size)
{
	return urealloc(ptr, size);
}