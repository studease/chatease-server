/*
 * stu_spinlock.c
 *
 *  Created on: 2016-11-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

void
stu_spinlock_init(stu_spinlock_t *lock) {
	stu_atomic_release(&lock->rlock);
}

void
stu_spin_lock(stu_spinlock_t *lock) {
	stu_uint_t  ticket;

	ticket = stu_atomic_fetch_add(&lock->rlock, STU_SPINLOCK_TICKET_UNIT) >> 16;
	for ( ; ticket != (stu_atomic_read(&lock->rlock) & STU_SPINLOCK_OWNER_MASK); ) {
		/* void */
	}
}

void
stu_spin_unlock(stu_spinlock_t *lock) {
	stu_atomic_fetch_add(&lock->rlock, 1);
}

