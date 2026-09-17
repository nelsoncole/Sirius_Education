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