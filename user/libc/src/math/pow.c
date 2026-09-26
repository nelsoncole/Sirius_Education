/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: pow.c
 *    Description: Função de exponenciação (pow) em Assembly Inline x87.
 *                 Totalmente blindada contra otimizações agressivas de -O2.
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * ============================================================================
 */

#include <math.h>

/**
 * pow - Calcula a potência de x elevado a y (x^y) de forma nativa pela FPU.
 * @x: A base.
 * @y: O expoente.
 */
double pow(double x, double y)
{
    // CASO ESPECIAL A: Qualquer número elevado a 0 é sempre 1.0 (Exigência IEEE 754)
    if (y == 0.0) {
        return 1.0;
    }

    // CASO ESPECIAL B: Se a base for 0.0
    if (x == 0.0) {
        // 0 elevado a qualquer número positivo é 0.0. 
        // 0 elevado a um número negativo tende ao infinito (tratado simplificado aqui como 0)
        return 0.0; 
    }

    // CASO ESPECIAL C: Se a base for negativa e o expoente não for inteiro,
    // o resultado entra no plano dos números complexos (não suportado por double puro).
    if (x < 0.0) {
        // Fallback defensivo simples para evitar que a FPU congele o Kernel/Ring 3
        return 0.0; 
    }

    double ret;

    // CORREÇÃO DE CLOBBERS E RESTRIÇÕES: 
    // Usamos o padrão estável de carregar explicitamente x e y na pilha da FPU 
    // ("t" força o topo st(0) e "u" força o st(1) de forma síncrona e inequívoca).
    __asm__ __volatile__ (
        "fyl2x;"            // st(0) = y * log2(x), limpa st(1)
        "fld %%st(0);"      // Duplica o topo: st(0) = topo, st(1) = topo
        "frndint;"          // Arredonda st(0) para o inteiro mais próximo (i)
        "fsubr %%st, %%st(1);" // st(1) = st(1) - st(0) -> obtém a parte fracionária (f)
        "fxch;"             // Troca st(0) com st(1). Agora st(0) = f, st(1) = i
        "f2xm1;"            // st(0) = 2^f - 1
        "fld1;"             // Carrega 1.0 no topo -> st(0) = 1.0, st(1) = 2^f - 1
        "faddp;"            // st(0) = (2^f - 1) + 1.0 = 2^f. st(1) passa a ser 'i'
        "fscale;"           // st(0) = 2^f * 2^i = 2^(f+i) = 2^(y * log2(x)) = x^y!
        "fstp %%st(1);"     // Limpa o lixo que sobrou no st(1) desimpedindo a FPU
        : "=t"(ret)         // O resultado final sai do topo st(0) para 'ret'
        : "0"(x), "u"(y)    // Garante que x está no topo st(0) e y está no st(1) antes do fyl2x
        : "st(1)", "cc", "memory" // CORREÇÃO: Avisa o -O2 que alteras as flags e a RAM!
    );

    return ret;
}