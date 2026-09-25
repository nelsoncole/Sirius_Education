#include <stdlib.h>
#include <stdio.h>

int system(const char *string)
{
	(void)string;
	printf("call system funtion error. \n");	
	return -1;
}
