/*
 * stu_spinlock.h
 *
 *  Created on: 2016-10-26
 *      Author: Tony Lau
 */

#ifndef STU_SPINLOCK_H_
#define STU_SPINLOCK_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_SPINLOCK_TICKET_UNIT 0x00010000
#define STU_SPINLOCK_OWNER_MASK  0x0000FFFF

typedef struct {
	stu_atomic_t  rlock;
} stu_spinlock_t;


void stu_spinlock_init(stu_spinlock_t *lock);

void stu_spin_lock(stu_spinlock_t *lock);
void stu_spin_unlock(stu_spinlock_t *lock);

#define stu_trylock(rlock)  ((rlock)->counter == 0 && stu_atomic_cmp_set(&(rlock)->counter, 0, 1))
#define stu_unlock(rlock)    (rlock)->counter = 0

#endif /* STU_SPINLOCK_H_ */
