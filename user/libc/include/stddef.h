#ifndef _STDDEF_H_
#define _STDDEF_H_


#include "size_t.h"
#include "ssize_t.h"

typedef signed ptrdiff_t;

typedef short unsigned int wchar_t;

#define NULL ((void *)0)

#define offsetof(type, member_designator) ((size_t) &((type *)0)->member_designator)


#endif
