/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: kpaging.h
 *    Description: Interface abstrata de controlo do hardware de paginação.
 *                 Atua como um wrapper (encapsulamento) portável, isolando
 *                 as operações de baixo nível de comutação de tabelas e 
 *                 invalidação de TLB de acordo com a arquitetura ativa.
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

#ifndef _KPAGING_H_
#define _KPAGING_H_

#if defined(__x86_64__) || defined(_M_X64)
    #include <kernel/arch/x86_64/mm/paging.h>
#elif defined(__aarch64__)
    #include <kernel/arch/arm64/mm/paging.h>
#else
    #error "Arquitetura nao suportada pelo mecanismo de paginacao (Paging)."
#endif

#endif /* _PAGING_H_ */
