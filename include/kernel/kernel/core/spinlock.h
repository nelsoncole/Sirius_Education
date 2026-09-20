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

#define SPINLOCK_RELEASED   0

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

/* Protótipos das funções nativas de trancamento */
void spinlock_acquire(spinlock_t* lock);
void spinlock_release(spinlock_t* lock);

/* Trincos blindados contra interrupções - Substitua pelas funções equivalentes da kapi.h do Sirius */
static inline unsigned long spinlock_lock_irqsave(spinlock_t* lock) {
    unsigned long flags;
    /* 1. Desativa interrupções locais do núcleo atual e guarda o estado anterior */
    __asm__ __volatile__("pushf; pop %0; cli" : "=g"(flags) :: "memory");
    spinlock_acquire(lock);
    return flags;
}

static inline void spinlock_unlock_irqrestore(spinlock_t* lock, unsigned long flags) {
    spinlock_release(lock);
    /* 2. Restaura o estado anterior das interrupções */
    __asm__ __volatile__("push %0; popf" : : "g"(flags) : "memory", "cc");
}


#endif