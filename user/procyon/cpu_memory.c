
/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: memcpy.c
 *    Description: User Space Memory Operations - SSE2 / AVX2
 *
 *         Author: Nelson Cole
 *        License: MIT
 * ============================================================================
 */

#include <string.h>

#include "cpu_memory.h"

static cpu_features_t cpu_features;

/* ============================================================================
 * CPUID / XGETBV
 * ============================================================================
 */

static inline void cpuid(
    uint32_t leaf,
    uint32_t subleaf,
    uint32_t *eax,
    uint32_t *ebx,
    uint32_t *ecx,
    uint32_t *edx)
{
    __asm__ volatile(
        "cpuid"
        : "=a"(*eax),
          "=b"(*ebx),
          "=c"(*ecx),
          "=d"(*edx)
        : "a"(leaf),
          "c"(subleaf)
    );
}

static inline uint64_t xgetbv(uint32_t index)
{
    uint32_t eax, edx;

    __asm__ volatile(
        "xgetbv"
        : "=a"(eax), "=d"(edx)
        : "c"(index)
    );

    return ((uint64_t)edx << 32) | eax;
}

static void cpu_detect_features(void)
{
    uint32_t eax, ebx, ecx, edx;

    cpu_features.sse2 = 0;
    cpu_features.avx = 0;
    cpu_features.avx2 = 0;
    cpu_features.osxsave = 0;

    cpuid(0, 0, &eax, &ebx, &ecx, &edx);

    uint32_t max_leaf = eax;

    if (max_leaf < 1)
        return;

    cpuid(1, 0, &eax, &ebx, &ecx, &edx);

    cpu_features.sse2 = (edx & (1U << 26)) != 0;
    cpu_features.osxsave = (ecx & (1U << 27)) != 0;

    if (!(ecx & (1U << 28)))
        return;

    if (!cpu_features.osxsave)
        return;

    uint64_t xcr0 = xgetbv(0);

    if ((xcr0 & 0x6) != 0x6)
        return;

    cpu_features.avx = 1;

    if (max_leaf < 7)
        return;

    cpuid(7, 0, &eax, &ebx, &ecx, &edx);

    cpu_features.avx2 = (ebx & (1U << 5)) != 0;
}


/* ============================================================================
 * SSE2 MEMCPY
 * ============================================================================
 */

static void *sse_memcpy(void *dst, const void *src, size_t len)
{
    void *ret = dst;

    unsigned char *d = dst;
    const unsigned char *s = src;

    // Fallback para buffers não alinhados a 16 bytes
    if (((unsigned long)d % 16) != 0 || ((unsigned long)s % 16) != 0) {
		size_t qwords = len / 8;
		size_t bytes = len % 8;
		uint64_t *d64 = (uint64_t *)d;
		uint64_t *s64 = (uint64_t *)s;

		__asm__ __volatile__(
			"cld; rep movsq"
			: "+D"(d64), "+S"(s64), "+c"(qwords) : : "memory");

		d = (unsigned char *)d64;
		s = (unsigned char *)s64;

		if (bytes) memcpy(d, s, bytes);
		return ret;
	}

    size_t blocks = len / 128;

    for (size_t i = 0; i < blocks; i++)
    {
        __asm__ volatile(
            "movdqu   0(%1), %%xmm0\n"
            "movdqu  16(%1), %%xmm1\n"
            "movdqu  32(%1), %%xmm2\n"
            "movdqu  48(%1), %%xmm3\n"
            "movdqu  64(%1), %%xmm4\n"
            "movdqu  80(%1), %%xmm5\n"
            "movdqu  96(%1), %%xmm6\n"
            "movdqu 112(%1), %%xmm7\n"

            "movdqa %%xmm0,   0(%0)\n"
            "movdqa %%xmm1,  16(%0)\n"
            "movdqa %%xmm2,  32(%0)\n"
            "movdqa %%xmm3,  48(%0)\n"
            "movdqa %%xmm4,  64(%0)\n"
            "movdqa %%xmm5,  80(%0)\n"
            "movdqa %%xmm6,  96(%0)\n"
            "movdqa %%xmm7, 112(%0)\n"
            :
            : "r"(d), "r"(s)
            : "memory",
              "xmm0", "xmm1", "xmm2", "xmm3",
              "xmm4", "xmm5", "xmm6", "xmm7"
        );

        d += 128;
        s += 128;
    }

    size_t remaining = len % 128;

    memcpy(d, s, remaining);

    return ret;
}

/* ============================================================================
 * AVX2 MEMCPY
 * ============================================================================
 */

__attribute__((target("avx2")))
static void *avx2_memcpy(void *dst, const void *src, size_t len)
{
    void *ret = dst;

    unsigned char *d = dst;
    const unsigned char *s = src;

    /* Fallback se os buffers nao estiverem alinhados ao limite natural de 32 bytes do AVX */
    if (((unsigned long)d % 32) != 0 || ((unsigned long)s % 32) != 0) {
		size_t qwords = len / 8;
		size_t bytes = len % 8;
		uint64_t *d64 = (uint64_t *)d;
		uint64_t *s64 = (uint64_t *)s;

		__asm__ __volatile__(
			"cld; rep movsq"
			: "+D"(d64), "+S"(s64), "+c"(qwords) : : "memory");

		d = (unsigned char *)d64;
		s = (unsigned char *)s64;

		if (bytes) memcpy(d, s, bytes);
		return ret;
	}

    size_t blocks = len / 256;

    for (size_t i = 0; i < blocks; i++)
    {
        __asm__ volatile(
            "vmovdqu   0(%1), %%ymm0\n"
            "vmovdqu  32(%1), %%ymm1\n"
            "vmovdqu  64(%1), %%ymm2\n"
            "vmovdqu  96(%1), %%ymm3\n"
            "vmovdqu 128(%1), %%ymm4\n"
            "vmovdqu 160(%1), %%ymm5\n"
            "vmovdqu 192(%1), %%ymm6\n"
            "vmovdqu 224(%1), %%ymm7\n"

            "vmovdqu %%ymm0,   0(%0)\n"
            "vmovdqu %%ymm1,  32(%0)\n"
            "vmovdqu %%ymm2,  64(%0)\n"
            "vmovdqu %%ymm3,  96(%0)\n"
            "vmovdqu %%ymm4, 128(%0)\n"
            "vmovdqu %%ymm5, 160(%0)\n"
            "vmovdqu %%ymm6, 192(%0)\n"
            "vmovdqu %%ymm7, 224(%0)\n"
            :
            : "r"(d), "r"(s)
            : "memory",
              "ymm0", "ymm1", "ymm2", "ymm3",
              "ymm4", "ymm5", "ymm6", "ymm7"
        );

        d += 256;
        s += 256;
    }

    size_t remaining = len % 256;

    memcpy(d, s, remaining);

    __asm__ volatile("vzeroupper");

    return ret;
}

void memcpy_init(void)
{
    cpu_detect_features();
}

void *optimized_memcpy(void *dst, const void *src, size_t bytes)
{
    if (dst == src || bytes == 0)
        return dst;

    if (!dst || !src)
        return dst;

    if (bytes < 64)
        return memcpy(dst, src, bytes);

    if (cpu_features.avx2)
        return avx2_memcpy(dst, src, bytes);

    if (cpu_features.sse2)
        return sse_memcpy(dst, src, bytes);

    return memcpy(dst, src, bytes);
}
