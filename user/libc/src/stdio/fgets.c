#include <stdio.h>

char *fgets (char *str,int length,FILE *fp)
{
	if(!fp) return str;
	
	int  c;
	char *s  = str;
	char *p  = str;
	
	int of = 0;
	
	while(1){
	
		c = fgetc (fp);
		if(c == EOF) break;
		
		if(c == '\n'){
            *s ++ = c;
            break;
        }
		
		if(of < length ) {
			
			if(c == '\b' && of > 0 && s > p ) { 
				s --;
				of --;
			}else if(c != '\b'){
			
				*s ++ = c;
				of ++;
			}
	
		}else {
			if(c == '\b' && of > 0) of --;
			else of ++;
		}		
	}
	
	if (length > 1) *s = '\0';
	else *s = c;
	
	return (char*)str;
}

