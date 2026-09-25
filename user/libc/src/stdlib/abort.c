#include <stdlib.h>


#undef        abort

void abort(void)
{
	exit(1);
}
