#include <stdlib.h>
#include <unistd.h>


#undef        exit

void exit(int status)
{
	_exit(status);
}
