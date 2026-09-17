/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: spinlock.h
 *    Description: Primitivas de sincronização atómica (Spinlocks) para SMP.
 * 
 *        Author:  Nelson Cole
 *   Created Date: 15/09/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 15/09/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _SPINLOCK_H_
#define _SPINLOCK_H_

/**
 * Estrutura de um Spinlock Primitivo para Proteção SMP
 */
typedef struct {
    volatile int lock;
} spinlock_t;

/**
 * spin_lock_init - Inicializa o estado do trinco como livre.
 */
void spin_lock_init(spinlock_t *lock);

/**
 * spin_lock - Adquire o trinco de forma atómica. Bloqueia em loop (spinning)
 *             se o trinco estiver ocupado por outro núcleo de processamento.
 */
void spin_lock(spinlock_t *lock);

/**
 * spin_unlock - Liberta o trinco de forma atómica para os restantes núcleos.
 */
void spin_unlock(spinlock_t *lock);

#endif