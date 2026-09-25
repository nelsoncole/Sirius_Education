#include <string.h>
#include <stdio.h>

char *strerror(int errnum){
	(void)errnum;
	printf("strerrorr\n");
	for(;;);
	return 0;
}
