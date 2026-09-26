#include <stdio.h>

int setvbuf(FILE * restrict stream, char * restrict buf, int mode, size_t size){

    (void)stream;
    (void)buf;
    (void)mode;
    (void)size;
    
    printf("setvbuf...");
    while(1){}
	return 0;


}


/*
#include <stdio.h>
#include <stdlib.h>

int setvbuf(FILE * restrict stream, char * restrict buf, int mode, size_t size) {
    if (!stream) return -1;

    // Escolha do buffer
    if (buf) {
        stream->_base = (unsigned char*)buf;
        stream->_bufsiz = size;
    } else {
        // Aloca buffer internamente se buf==NULL
        stream->_base = malloc(size);
        if (!stream->_base) return -1;
        stream->_bufsiz = size;
    }

    stream->_ptr = stream->_base;

    // Define modo
    stream->_flags &= ~(_IONBF|_IOLBF|_IOFBF);
    stream->_flags |= mode;

    return 0;
}*/