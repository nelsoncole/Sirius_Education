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