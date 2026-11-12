/*
 * ============================================================================
 *        Project: Sirius_Education
 *       Filename: wait.h
 *    Description: 
 * 
 *         Author: Nelson Cole
 *   Created Date: 24/10/2026
 * 
 *    Modified By: Nelson Cole
 *  Modified Date: 25/10/2026
 * 
 *        License: MIT
 * ============================================================================
 */

#ifndef _WAIT_H_
#define _WAIT_H_

#include <kernel/kernel/core/spinlock.h>
#include <kernel/arch/x86_64/cpu/cpu.h>
#include <kernel/kernel/sched/thread.h>

typedef struct wait_queue {
    cpu_data_block_t* cpu;
    thread_t* thread;
    void* chan;
    struct wait_queue* next;
} wait_queue_t;


wait_queue_t *wait_queue_remove(void *chan);
void wait(void *chan, spinlock_t *lk);
void wait_no_queue(void *chan, spinlock_t *lk);

#endif
