#include <sys/uheap.h>
#include <stdlib.h>

#undef        malloc

void *malloc(size_t size) {

	return umalloc(size);
}