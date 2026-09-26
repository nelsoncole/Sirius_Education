/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sqrt.c
 *    Description: Função de raiz quadrada (sqrt) segura em Assembly Inline x87.
 *                 Totalmente imune a efeitos colaterais de reordenação em -O2.
 * 
 *         Author: Nelson Cole
 *   Created Date: 26/09/2026
 * ============================================================================
 */

#include <math.h>

/**
 * sqrt - Calcula a raiz quadrada de x de forma nativa pela FPU x87.
 * @x: O valor de entrada.
 */
double sqrt(double x)
{
    // CASO ESPECIAL: Raiz quadrada de número negativo não existe nos números reais.
    // Retorna 0.0 (ou podrías retornar NAN se tiveres definido no math.h)
    // para evitar que a FPU dispare uma exceção de hardware não tratada no teu Kernel.
    if (x < 0.0) {
        return 0.0; 
    }

    double ret;

    // REMOVIDO: "finit;". A FPU deve ser inicializada apenas UMA vez no boot do processo (cr0/cr4).
    // Mudámos a restrição de "m"(x) para "0"(x) combinada com "=t", o que diz ao GCC 
    // para passar 'x' diretamente no topo da pilha st(0), evitando acessos lentos à RAM.
    __asm__ __volatile__ (
        "fsqrt;"
        : "=t"(ret)               // O resultado final sai do topo st(0) para 'ret'
        : "0"(x)                  // Força x a entrar exatamente no topo st(0)
        : "cc", "memory"          // CORREÇÃO: Avisa o -O2 que alteras as flags da FPU e a RAM
    );

    return ret;
}