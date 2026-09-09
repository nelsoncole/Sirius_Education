/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kvmm.h
 *    Description: Interface abstrata do Gestor de Memória Virtual (VMM).
 *                 Atua como um wrapper (encapsulamento) portável, isolando
 *                 os drivers genéricos das dependências diretas de hardware.
 *                 Inclui condicionalmente as estruturas de paginação nativas
 *                 da arquitetura ativa (ex: x86_64).
 * 
 *         Author: Nelson Cole
 *   Created Date: 08/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 08/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _KVMM_H_
#define _KVMM_H_

#include <kernel/kernel/mm/memory_map.h>

#if defined(__x86_64__) || defined(_M_X64)
    #include <kernel/arch/x86_64/mm/vmm.h>
#elif defined(__aarch64__)
    #include <kernel/arch/arm64/mm/vmm.h>
#else
    #error "Arquitetura nao suportada pelo gestor de memoria virtual (VMM)."
#endif

#endif /* _VMM_H_ */
