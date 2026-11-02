#ifndef UTF8_DECODE_H
#define UTF8_DECODE_H

#include <stdint.h>
#include <stddef.h>


int utf8_decode(const char *utf8, uint32_t *codepoint);
size_t utf8_decode_string(const char *utf8, uint32_t *output, size_t output_capacity);


#endif
