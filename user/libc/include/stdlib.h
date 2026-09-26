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

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);


div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
lldiv_t lldiv(long long numer,long long denom);


long int strtol(const char *nptr,char **endptr, int base);
int atoi (const char *nptr);
long atol(const char *nptr);

long long int strtoul ( const char *nptr, char **endptr, int base);

void exit(int rc);
char *getenv(const char *name);

void abort(void);

void qsort(void *base, size_t nmemb, size_t size,int (*compar)(const void *, const void *));

int abs ( int j);

double strtod(const char *nptr, char ** endptr);
float strtof(const char *str, char **endptr);
double atof(const char *nptr);
long double strtold(const char *str, char **endptr);

int system(const char *string);
void srand(unsigned int seed);
int rand(void);

#endif