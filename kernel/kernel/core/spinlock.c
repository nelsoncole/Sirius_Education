/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: spinlock.c
 *    Description: Implementação das primitivas de Spinlock globais do sistema.
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

#include <kernel/kernel/core/spinlock.h>

void spin_lock_init(spinlock_t *lock) {
    if (!lock) return;
    lock->lock = 0;
}

void spin_lock(spinlock_t *lock) {
    if (!lock) return;
    /* Loop de espera ativa atómico com otimização de pipeline (pause) */
    while (__atomic_test_and_set(&(lock->lock), __ATOMIC_ACQUIRE)) {
        __asm__ __volatile__("pause");
    }
}

void spin_unlock(spinlock_t *lock) {
    if (!lock) return;
    __atomic_clear(&(lock->lock), __ATOMIC_RELEASE);
}

void spinlock_acquire(spinlock_t* lock) 
{
    if (!lock) return;
    /* Loop de espera ativa atómico com otimização de pipeline (pause) */
    while (__atomic_test_and_set(&(lock->lock), __ATOMIC_ACQUIRE)) {
        __asm__ __volatile__("pause");
    }
}

void spinlock_release(spinlock_t* lock) 
{
     if (!lock) return;
    __atomic_clear(&(lock->lock), __ATOMIC_RELEASE);
}


/* Trincos blindados contra interrupções - Substitua pelas funções equivalentes da kapi.h do Sirius */
unsigned long spinlock_lock_irqsave(spinlock_t* lock) {
    unsigned long flags;
    /* 1. Desativa interrupções locais do núcleo atual e guarda o estado anterior */
    __asm__ __volatile__("pushf; pop %0; cli" : "=g"(flags) :: "memory");
    spinlock_acquire(lock);
    return flags;
}

void spinlock_unlock_irqrestore(spinlock_t* lock, unsigned long flags) {
    spinlock_release(lock);
    /* 2. Restaura o estado anterior das interrupções */
    __asm__ __volatile__("push %0; popf" : : "g"(flags) : "memory", "cc");
}