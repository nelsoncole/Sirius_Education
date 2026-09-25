/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: stdlib.h
 *    Description: Protótipos padrão ISO C para gestão de memória dinâmica.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 24/09/2026
 * ============================================================================
 */

#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>

#define NULL ((void *)0)

typedef struct {
  int quot;
  int rem;
} div_t;


typedef struct {
  long int quot;
  long int rem;
} ldiv_t;


typedef struct {
  long long int quot;
  long long int rem;
} lldiv_t;


#define	EXIT_FAILURE 	1
#define	EXIT_SUCCESS 	0
#define	RAND_MAX	2147483647
#define	MB_CUR_MAX	1

extern void *malloc(size_t size);
extern void *calloc(size_t nmemb, size_t size);
extern void free(void *ptr);
extern void *realloc(void *ptr, size_t size);


extern div_t div(int numer, int denom);
extern ldiv_t ldiv(long numer, long denom);
extern lldiv_t lldiv(long long numer,long long denom);


extern long int strtol(const char *nptr,char **endptr, int base);
extern int atoi (const char *nptr);
extern long atol(const char *nptr);

extern	long long int strtoul ( const char *nptr, char **endptr, int base);

extern void exit(int rc);
extern char *getenv(const char *name);

extern void abort(void);

extern void qsort(void *base, size_t nmemb, size_t size,int (*compar)(const void *, const void *));

extern int abs ( int j);

extern double strtod(const char *nptr, char ** restrict endptr);
extern float strtof(const char *str, char **endptr);
extern double atof(const char *nptr);
extern long double strtold(const char *str, char **endptr);

// FIXME, nao funcionam
extern int system(const char *string);
extern void srand(unsigned int seed);
extern int rand(void);

#endif