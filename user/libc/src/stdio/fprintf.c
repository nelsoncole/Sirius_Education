#include <stdio.h>
#include <stdarg.h>


int fprintf(FILE *fp, const char *fmt, ...)
{
  	va_list ap;
  	va_start (ap, fmt);
  	int ret = vfprintf(fp, fmt, ap);
  	va_end (ap);
  	return ret;

}
