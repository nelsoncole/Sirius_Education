/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: qsort.c
 *    Description: Implementação do algoritmo Quicksort clássico (Unix V7)
 *                 ajustado e tipificado para conformidade estrita POSIX/ISO C
 *                 em ambientes de 64-bits (Ring 3).
 * 
 *        Credits: unix-v7 / Nelson Cole
 *   Created Date: 13/09/2026
 * ============================================================================
 */

#include <stdlib.h>
#include <stddef.h>

static int (*qscmp)(const void*, const void*);
static size_t qses; // CORREÇÃO: Tamanho do elemento deve ser sempre size_t

static void qsexc(char *i, char *j)
{
    register char *ri, *rj, c;
    size_t n; // CORREÇÃO: Contador sem sinal compatível com size_t

    n = qses;
    ri = i;
    rj = j;
    do {
        c = *ri;
        *ri++ = *rj;
        *rj++ = c;
    } while(--n);
}

static void qstexc(char *i, char *j, char *k)
{
    char *ri, *rj, *rk;
    char c; // CORREÇÃO: c armazena um caractere bruto (char), não int
    size_t n;

    n = qses;
    ri = i;
    rj = j;
    rk = k;
    do {
        c = *ri;
        *ri++ = *rk;
        *rk++ = *rj;
        *rj++ = c;
    } while(--n);
}

static void qs1(char *a, char *l)
{
    char *i, *j;
    size_t es; // CORREÇÃO: Sincronizado como size_t contra avisos de signedness
    char *lp, *hp;
    int c;
    size_t n;  // CORREÇÃO: Diferenças geométricas e índices mapeados em size_t

    es = qses;

start:
    // CORREÇÃO DA LINHA 57: l - a gera ptrdiff_t. O cast para size_t atém n.
    // Como es agora também é size_t, a comparação fica equilibrada (unsigned vs unsigned)
    if ((n = (size_t)(l - a)) <= es)
        return;
        
    n = es * (n / (2 * es));
    hp = lp = a + n;
    i = a;
    j = l - es;
    for(;;) {
        if(i < lp) {
            if((c = (*qscmp)(i, lp)) == 0) {
                qsexc(i, lp -= es);
                continue;
            }
            if(c < 0) {
                i += es;
                continue;
            }
        }

loop:
        if(j > hp) {
            if((c = (*qscmp)(hp, j)) == 0) {
                qsexc(hp += es, j);
                goto loop;
            }
            if(c > 0) {
                if(i == lp) {
                    qstexc(i, hp += es, j);
                    i = lp += es;
                    goto loop;
                }
                qsexc(i, j);
                j -= es;
                i += es;
                continue;
            }
            j -= es;
            goto loop;
        }

        if(i == lp) {
            if((size_t)(lp - a) >= (size_t)(l - hp)) {
                qs1(hp + es, l);
                l = lp;
            } else {
                qs1(a, lp);
                a = hp + es;
            }
            goto start;
        }

        qstexc(j, lp -= es, i);
        j = hp -= es;
    }
}

/**
 * qsort - Interface pública ISO C para ordenação de arrays.
 */
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
    if (!base || nmemb == 0 || size == 0 || !compar) return;

    qscmp = compar;
    qses = size;
    qs1((char*)base, (char*)base + nmemb * size);
}