#include <stdio.h>

extern int fgetc_r (FILE *fp);

int getc (FILE *fp)
{	
	if(!fp) return -1;
	
	int c = fgetc_r (fp);
	
	return (c);

}

