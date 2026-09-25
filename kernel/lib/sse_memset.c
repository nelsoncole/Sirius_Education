/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sse_memset.c
 *    Description: Rotinas ultra-otimizadas de preenchimento de memória (memset) 
 *                 utilizando extensões SIMD (SSE/XMM) de 128 bits para x86_64.
 *                 Implementa Loop Unrolling de 128 bytes e escrita não-temporal.
 * 
 *         Author: Nelson Cole
 *   Created Date: 18/09/2026
 *
 *    Modified By: Nelson Cole
 *  Modified Date: 21/09/2026
 *  
 *        License: MIT
 * ============================================================================
 */

#include <kernel/lib/stdint.h>
#include <kernel/lib/stddef.h>

#define SSE_MMREG_SIZE 16

void *sse_memset(void *dst, int value, size_t len) {
    void *ret = dst;
    size_t i = 0;

    uint8_t val = (uint8_t)value;
    uint8_t buffer[16] __attribute__((aligned(16)));
    for (i = 0; i < 16; i++) buffer[i] = val;

    uint8_t *ptr = (uint8_t *)dst;

    // Alinhamento manual inicial de 16 bytes
    while (((unsigned long)ptr % SSE_MMREG_SIZE) && len > 0) {
        *ptr++ = val;
        len--;
    }

    // SUPER LOOP: Processa 128 Bytes por iteração com escritas não-temporais
    size_t blocks = len / 128;
    if (blocks > 0) {
        __asm__ __volatile__ (
            "movdqu (%0), %%xmm0\n"
            "movaps %%xmm0, %%xmm1\n"
            "movaps %%xmm0, %%xmm2\n"
            "movaps %%xmm0, %%xmm3\n"
            "movaps %%xmm0, %%xmm4\n"
            "movaps %%xmm0, %%xmm5\n"
            "movaps %%xmm0, %%xmm6\n"
            "movaps %%xmm0, %%xmm7\n"
            : : "r"(buffer) : "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7"
        );

        for (i = 0; i < blocks; i++) {
            __asm__ __volatile__ (
                "movntdq %%xmm0, (%0)\n"
                "movntdq %%xmm1, 16(%0)\n"
                "movntdq %%xmm2, 32(%0)\n"
                "movntdq %%xmm3, 48(%0)\n"
                "movntdq %%xmm4, 64(%0)\n"
                "movntdq %%xmm5, 80(%0)\n"
                "movntdq %%xmm6, 96(%0)\n"
                "movntdq %%xmm7, 112(%0)\n"
                : : "r"(ptr) : "memory"
            );
            ptr += 128;
        }
        __asm__ __volatile__("sfence" ::: "memory");
    }

    // Sobras menores que 128 bytes (processadas em blocos de 16 bytes)
    size_t rem_blocks = (len % 128) / 16;
    if (rem_blocks > 0) {
        for (i = 0; i < rem_blocks; i++) {
            __asm__ __volatile__ ("movntdq %%xmm0, (%0)\n" : : "r"(ptr) : "memory");
            ptr += 16;
        }
        __asm__ __volatile__("sfence" ::: "memory");
    }

    // Sobras finais inferiores a 16 bytes
    len = len % 16;
    while (len--) {
        *ptr++ = val;
    }

    return ret;
}

void *sse_memset_dword(void *dst, uint32_t value, size_t count) {
    typedef unsigned long uintptr_t;
    void *ret = dst;
    size_t i;

    uint32_t buffer[4] __attribute__((aligned(16)));
    for (i = 0; i < 4; i++) buffer[i] = value;

    uint8_t *ptr = (uint8_t *)dst;

    // Alinhamento manual inicial (Avança de 4 em 4 bytes até alinhar a 16)
    while (((uintptr_t)ptr % SSE_MMREG_SIZE) && count > 0) {
        *(uint32_t *)ptr = value;
        ptr += 4;
        count--;
    }

    // SUPER LOOP DWORD: Limpa 32 píxeis (128 Bytes) de cada vez à velocidade máxima
    size_t blocks = count / 32; // 32 dwords = 128 bytes
    if (blocks > 0) {
        __asm__ __volatile__ (
            "movups (%0), %%xmm0\n"
            "movaps %%xmm0, %%xmm1\n"
            "movaps %%xmm0, %%xmm2\n"
            "movaps %%xmm0, %%xmm3\n"
            "movaps %%xmm0, %%xmm4\n"
            "movaps %%xmm0, %%xmm5\n"
            "movaps %%xmm0, %%xmm6\n"
            "movaps %%xmm0, %%xmm7\n"
            : : "r"(buffer) : "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7"
        );

        for (i = 0; i < blocks; i++) {
            // Alterado para movntdq para fazer Streaming direto na VRAM
            __asm__ __volatile__ (
                "movntdq %%xmm0, (%0)\n"
                "movntdq %%xmm1, 16(%0)\n"
                "movntdq %%xmm2, 32(%0)\n"
                "movntdq %%xmm3, 48(%0)\n"
                "movntdq %%xmm4, 64(%0)\n"
                "movntdq %%xmm5, 80(%0)\n"
                "movntdq %%xmm6, 96(%0)\n"
                "movntdq %%xmm7, 112(%0)\n"
                : : "r"(ptr) : "memory"
            );
            ptr += 128;
        }
        __asm__ __volatile__("sfence" ::: "memory");
    }

    // Sobras menores que 32 dwords (processadas em blocos de 4 dwords / 16 bytes)
    size_t rem_blocks = (count % 32) / 4;
    if (rem_blocks > 0) {
        for (i = 0; i < rem_blocks; i++) {
            __asm__ __volatile__ ("movntdq %%xmm0, (%0)\n" : : "r"(ptr) : "memory");
            ptr += 16;
        }
        __asm__ __volatile__("sfence" ::: "memory");
    }

    // Sobras finais de píxeis
    count %= 4;
    while (count--) {
        *(uint32_t *)ptr = value;
        ptr += 4;
    }

    return ret;
}

#define AVX2_MMREG_SIZE 32

/**
 * avx2_memset - Preenche um bloco de memória baseado em BYTES (8 bits).
 *              Ideal para limpezas genéricas de buffers massivos.
 */
void *avx2_memset(void *dst, int value, size_t len) {
    void *ret = dst;
    size_t i = 0;

    uint8_t val = (uint8_t)value;
    uint8_t buffer[32] __attribute__((aligned(32)));
    for (i = 0; i < 32; i++) buffer[i] = val;

    uint8_t *ptr = (uint8_t *)dst;

    // 1. Alinhamento manual inicial de 32 bytes
    while (((unsigned long)ptr % AVX2_MMREG_SIZE) && len > 0) {
        *ptr++ = val;
        len--;
    }

    // 2. SUPER LOOP: Processa 256 Bytes por iteração com escritas não-temporais AVX2
    size_t blocks = len / 256;
    if (blocks > 0) {
        __asm__ __volatile__ (
            "vmovdqu (%0), %%ymm0\n"
            "vmovaps %%ymm0, %%ymm1\n"
            "vmovaps %%ymm0, %%ymm2\n"
            "vmovaps %%ymm0, %%ymm3\n"
            "vmovaps %%ymm0, %%ymm4\n"
            "vmovaps %%ymm0, %%ymm5\n"
            "vmovaps %%ymm0, %%ymm6\n"
            "vmovaps %%ymm0, %%ymm7\n"
            : : "r"(buffer) : "ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7"
        );

        for (i = 0; i < blocks; i++) {
            __asm__ __volatile__ (
                "vmovntdq %%ymm0, (%0)\n"
                "vmovntdq %%ymm1, 32(%0)\n"
                "vmovntdq %%ymm2, 64(%0)\n"
                "vmovntdq %%ymm3, 96(%0)\n"
                "vmovntdq %%ymm4, 128(%0)\n"
                "vmovntdq %%ymm5, 160(%0)\n"
                "vmovntdq %%ymm6, 192(%0)\n"
                "vmovntdq %%ymm7, 224(%0)\n"
                : : "r"(ptr) : "memory"
            );
            ptr += 256;
        }
        __asm__ __volatile__("sfence" ::: "memory");
    }

    // 3. Sobras menores que 256 bytes (processadas em blocos de 32 bytes)
    size_t rem_blocks = (len % 256) / 32;
    if (rem_blocks > 0) {
        for (i = 0; i < rem_blocks; i++) {
            __asm__ __volatile__ ("vmovntdq %%ymm0, (%0)\n" : : "r"(ptr) : "memory");
            ptr += 32;
        }
        __asm__ __volatile__("sfence" ::: "memory");
    }

    // 4. Sobras finais inferiores a 32 bytes
    len = len % 32;
    while (len--) {
        *ptr++ = val;
    }

    return ret;
}

/**
 * avx2_memset_dword - Preenche um bloco de memória baseado em DWORDS (32 bits).
 *                    Ideal para preencher/limpar buffers de pixéis gráficos.
 */
void *avx2_memset_dword(void *dst, uint32_t value, size_t count) {
    typedef unsigned long uintptr_t;
    void *ret = dst;
    size_t i;

    uint32_t buffer[8] __attribute__((aligned(32)));
    for (i = 0; i < 8; i++) buffer[i] = value;

    uint8_t *ptr = (uint8_t *)dst;

    // 1. Alinhamento manual inicial (Avança de 4 em 4 bytes até alinhar a 32)
    while (((uintptr_t)ptr % AVX2_MMREG_SIZE) && count > 0) {
        *(uint32_t *)ptr = value;
        ptr += 4;
        count--;
    }

    // 2. SUPER LOOP DWORD: Limpa 64 píxeis (256 Bytes) de cada vez à velocidade máxima
    size_t blocks = count / 64; // 64 dwords = 256 bytes
    if (blocks > 0) {
        __asm__ __volatile__ (
            "vmovdqu (%0), %%ymm0\n"
            "vmovaps %%ymm0, %%ymm1\n"
            "vmovaps %%ymm0, %%ymm2\n"
            "vmovaps %%ymm0, %%ymm3\n"
            "vmovaps %%ymm0, %%ymm4\n"
            "vmovaps %%ymm0, %%ymm5\n"
            "vmovaps %%ymm0, %%ymm6\n"
            "vmovaps %%ymm0, %%ymm7\n"
            : : "r"(buffer) : "ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7"
        );

        for (i = 0; i < blocks; i++) {
            __asm__ __volatile__ (
                "vmovntdq %%ymm0, (%0)\n"
                "vmovntdq %%ymm1, 32(%0)\n"
                "vmovntdq %%ymm2, 64(%0)\n"
                "vmovntdq %%ymm3, 96(%0)\n"
                "vmovntdq %%ymm4, 128(%0)\n"
                "vmovntdq %%ymm5, 160(%0)\n"
                "vmovntdq %%ymm6, 192(%0)\n"
                "vmovntdq %%ymm7, 224(%0)\n"
                : : "r"(ptr) : "memory"
            );
            ptr += 256;
        }
        __asm__ __volatile__("sfence" ::: "memory");
    }

    // 3. Sobras menores que 64 dwords (processadas em blocos de 8 dwords / 32 bytes)
    size_t rem_blocks = (count % 64) / 8;
    if (rem_blocks > 0) {
        for (i = 0; i < rem_blocks; i++) {
            __asm__ __volatile__ ("vmovntdq %%ymm0, (%0)\n" : : "r"(ptr) : "memory");
            ptr += 32;
        }
        __asm__ __volatile__("sfence" ::: "memory");
    }

    // 4. Sobras finais de píxeis (inferiores a 8 dwords)
    count %= 8;
    while (count--) {
        *(uint32_t *)ptr = value;
        ptr += 4;
    }

    return ret;
}

// Variável global controlada pelo código de deteção de CPUID no boot
extern int g_cpu_has_avx2;
/**
 * optimized_memset - Preenche um bloco de memória baseado em BYTES selecionando 
 *                    dinamicamente a melhor extensão SIMD disponível no hardware.
 */
void *optimized_memset(void *dst, int value, size_t bytes) {
    if (!dst || bytes == 0) return dst;

    /* 
     * HEURÍSTICA DE TAMANHO: Para blocos pequenos (ex: menos de 64 bytes),
     * o overhead de espalhar o byte pelos registadores XMM/YMM não compensa.
     * Um laço simples e direto em C limpa a região com máxima eficiência.
     */
    if (bytes < 64) {
        uint8_t *d = (uint8_t *)dst;
        uint8_t v = (uint8_t)value;
        for (size_t i = 0; i < bytes; i++) {
            d[i] = v;
        }
        return dst;
    }

    /* 
     * SELEÇÃO DINÂMICA: Despacha para a rotina vetorial baseada na
     * capacidade real reportada pelo CPUID no arranque do Sirius OS.
     */
    if (g_cpu_has_avx2) {
        avx2_memset(dst, value, bytes);
    } else {
        sse_memset(dst, value, bytes);
    }

    return dst;
}

/**
 * optimized_memset_dword - Preenche um bloco de memória baseado em DWORDS (32-bit Pixels)
 *                          selecionando dinamicamente a melhor extensão SIMD no hardware.
 * @count: Quantidade de DWORDS (Não em bytes!) a serem preenchidos.
 */
void *optimized_memset_dword(void *dst, uint32_t value, size_t count) {
    if (!dst || count == 0) return dst;

    /* 
     * HEURÍSTICA DE TAMANHO: Se fores pintar menos de 16 píxeis (64 bytes),
     * não compensa o chaveamento dos registadores vetoriais. 
     * Um laço DWORD linear em C é mais limpo e rápido.
     */
    if (count < 16) {
        uint32_t *d = (uint32_t *)dst;
        for (size_t i = 0; i < count; i++) {
            d[i] = value;
        }
        return dst;
    }

    /*
     * SELEÇÃO DINÂMICA DE 32-BITS: Chaveia entre AVX2 (YMM - 256 bits) 
     * ou o teu sse_memset_dword (XMM - 128 bits) que usa escrita não-temporal.
     */
    if (g_cpu_has_avx2) {
        avx2_memset_dword(dst, value, count);
    } else {
        sse_memset_dword(dst, value, count);
    }

    return dst;
}