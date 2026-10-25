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
void wait(void *chan, spinlock_t *lk);
void wait_flag(void *chan, spinlock_t *lk, unsigned long flags);

#endif
