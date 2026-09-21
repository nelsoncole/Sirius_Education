/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: sse_memcpy.c
 *    Description: Rotinas ultra-otimizadas de cópia de memória (memcpy) via
 *                 SSE de 128-bits com Loop Unrolling levado ao limite absoluto
 *                 (256 bytes por iteração) e Streaming Loads/Stores para x86_64.
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

#define small_memcpy(dst, src, n)                    \
{                                                    \
	register unsigned long dummy;                    \
	__asm__ __volatile__(                            \
		"rep; movsb"                                 \
		: "=&D"(dst), "=&S"(src), "=&c"(dummy)        \
		: "0"(dst), "1"(src), "2"(n)                  \
		: "memory");                                  \
}

#define SSE_MMREG_SIZE 16

void *sse_memcpy(void *s1, const void *s2, size_t len) {
	void *retval = s1;
	unsigned char *dst = (unsigned char *)s1;
	unsigned char *src = (unsigned char *)s2;

	// Fallback para buffers não alinhados a 16 bytes
	if (((unsigned long)dst % SSE_MMREG_SIZE) != 0 || ((unsigned long)src % SSE_MMREG_SIZE) != 0) {
		size_t qwords = len / 8;
		size_t bytes = len % 8;
		uint64_t *d64 = (uint64_t *)dst;
		uint64_t *s64 = (uint64_t *)src;

		__asm__ __volatile__(
			"cld; rep movsq"
			: "+D"(d64), "+S"(s64), "+c"(qwords) : : "memory");

		dst = (unsigned char *)d64;
		src = (unsigned char *)s64;

		if (bytes) small_memcpy(dst, src, bytes);
		return retval;
	}

	// ULTRA LOOP: Processa blocos massivos de 256 Bytes por ciclo
	size_t blocks = len / 256;
	for (size_t i = 0; i < blocks; i++) {
		__asm__ __volatile__ (
			// --- BLOCO 1: Primeiros 128 Bytes ---
			"movntdqa (%0), %%xmm0\n"
			"movntdqa 16(%0), %%xmm1\n"
			"movntdqa 32(%0), %%xmm2\n"
			"movntdqa 48(%0), %%xmm3\n"
			"movntdqa 64(%0), %%xmm4\n"
			"movntdqa 80(%0), %%xmm5\n"
			"movntdqa 96(%0), %%xmm6\n"
			"movntdqa 112(%0), %%xmm7\n"

			"movntdq %%xmm0, (%1)\n"
			"movntdq %%xmm1, 16(%1)\n"
			"movntdq %%xmm2, 32(%1)\n"
			"movntdq %%xmm3, 48(%1)\n"
			"movntdq %%xmm4, 64(%1)\n"
			"movntdq %%xmm5, 80(%1)\n"
			"movntdq %%xmm6, 96(%1)\n"
			"movntdq %%xmm7, 112(%1)\n"

			// --- BLOCO 2: Segundos 128 Bytes (Reutilização de registos) ---
			"movntdqa 128(%0), %%xmm0\n"
			"movntdqa 144(%0), %%xmm1\n"
			"movntdqa 160(%0), %%xmm2\n"
			"movntdqa 176(%0), %%xmm3\n"
			"movntdqa 192(%0), %%xmm4\n"
			"movntdqa 208(%0), %%xmm5\n"
			"movntdqa 224(%0), %%xmm6\n"
			"movntdqa 240(%0), %%xmm7\n"

			"movntdq %%xmm0, 128(%1)\n"
			"movntdq %%xmm1, 144(%1)\n"
			"movntdq %%xmm2, 160(%1)\n"
			"movntdq %%xmm3, 176(%1)\n"
			"movntdq %%xmm4, 192(%1)\n"
			"movntdq %%xmm5, 208(%1)\n"
			"movntdq %%xmm6, 224(%1)\n"
			"movntdq %%xmm7, 240(%1)\n"
			:
			: "r"(src), "r"(dst)
			: "memory", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7");

		dst += 256;
		src += 256;
	}

	// Força a descarga e ordenação global dos buffers não-temporais
	__asm__ __volatile__("sfence" ::: "memory");

	// Sobras inferiores a 256 bytes (tratadas em fatias intermédias de 16 bytes via SSE)
	size_t rem_blocks = (len % 256) / 16;
	for (size_t i = 0; i < rem_blocks; i++) {
		__asm__ __volatile__ (
			"movntdqa (%0), %%xmm0\n"
			"movntdq %%xmm0, (%1)\n"
			: : "r"(src), "r"(dst) : "memory", "xmm0"
		);
		dst += 16;
		src += 16;
	}

	if (rem_blocks > 0) {
		__asm__ __volatile__("sfence" ::: "memory");
	}

	// Limpa os resíduos finais (menor que 16 bytes)
	if (len % 16) {
		small_memcpy(dst, src, len % 16);
	}

	return retval;
}


#define AVX_MMREG_SIZE 32

#define small_memcpy(dst, src, n)                    \
{                                                    \
	register unsigned long dummy;                    \
	__asm__ __volatile__(                            \
		"rep; movsb"                                 \
		: "=&D"(dst), "=&S"(src), "=&c"(dummy)        \
		: "0"(dst), "1"(src), "2"(n)                  \
		: "memory");                                  \
}

void *avx2_memcpy(void *s1, const void *s2, size_t len) 
{
	void *retval = s1;
	unsigned char *dst = (unsigned char *)s1;
	unsigned char *src = (unsigned char *)s2;

	/* Fallback se os buffers nao estiverem alinhados ao limite natural de 32 bytes do AVX */
	if (((unsigned long)dst % AVX_MMREG_SIZE) != 0 || ((unsigned long)src % AVX_MMREG_SIZE) != 0) 
	{
		size_t qwords = len / 8;
		size_t bytes = len % 8;
		uint64_t *d64 = (uint64_t *)dst;
		uint64_t *s64 = (uint64_t *)src;

		__asm__ __volatile__(
			"cld; rep movsq"
			: "+D"(d64), "+S"(s64), "+c"(qwords) : : "memory");

		dst = (unsigned char *)d64;
		src = (unsigned char *)s64;

		if (bytes) 
		{
			small_memcpy(dst, src, bytes);
		}
		return retval;
	}

	/* ULTRA LOOP: Processa blocos massivos de 256 Bytes por ciclo (8 registos YMM de 32 bytes) */
	size_t blocks = len / 256;
	for (size_t i = 0; i < blocks; i++) 
	{
		__asm__ __volatile__ (
			/* Leituras tolerantes a desalinhamentos de linha de cache (Evita #GP / Triple Fault) */
			"vmovdqu (%0), %%ymm0\n"
			"vmovdqu 32(%0), %%ymm1\n"
			"vmovdqu 64(%0), %%ymm2\n"
			"vmovdqu 96(%0), %%ymm3\n"
			"vmovdqu 128(%0), %%ymm4\n"
			"vmovdqu 160(%0), %%ymm5\n"
			"vmovdqu 192(%0), %%ymm6\n"
			"vmovdqu 224(%0), %%ymm7\n"

			/* Escritas diretas na RAM (Non-Temporal) - Seguras devido ao alinhamento validado no 'if' */
			"vmovntdq %%ymm0, (%1)\n"
			"vmovntdq %%ymm1, 32(%1)\n"
			"vmovntdq %%ymm2, 64(%1)\n"
			"vmovntdq %%ymm3, 96(%1)\n"
			"vmovntdq %%ymm4, 128(%1)\n"
			"vmovntdq %%ymm5, 160(%1)\n"
			"vmovntdq %%ymm6, 192(%1)\n"
			"vmovntdq %%ymm7, 224(%1)\n"
			:
			: "r"(src), "r"(dst)
			: "memory", "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7");

		dst += 256;
		src += 256;
	}

	/* Forca a descarga e ordenacao global dos buffers nao-temporais na RAM */
	__asm__ __volatile__("sfence" ::: "memory");

	/* Sobras inferiores a 256 bytes tratadas de forma segura via vmovdqu/vmovdqu */
	size_t rem_blocks = (len % 256) / 32;
	for (size_t i = 0; i < rem_blocks; i++) 
	{
		__asm__ __volatile__ (
			"vmovdqu (%0), %%ymm0\n"
			"vmovdqu %%ymm0, (%1)\n" /* Alterado para escrita tolerante a desalinhamento residual */
			: : "r"(src), "r"(dst) : "memory", "ymm0"
		);
		dst += 32;
		src += 32;
	}

	/* Limpa os residuos finais inferiores a 32 bytes */
	if (len % 32) 
	{
		small_memcpy(dst, src, len % 32);
	}

	return retval;
}