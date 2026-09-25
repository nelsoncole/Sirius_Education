#include <stdlib.h>
#include <sys/uheap.h>

#undef        free

void free(void *ptr) {
	ufree(ptr);
}