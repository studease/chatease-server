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
	stu_atomic_release(&lock->rlock.counter);
}

void
stu_spin_lock(stu_spinlock_t *lock) {
	stu_uint_t  ticket;
	time_t      start, now;

	ticket = stu_atomic_fetch_add(&lock->rlock.counter, STU_SPINLOCK_TICKET_UNIT) >> 16;
	for (start = time(NULL); ticket != (stu_atomic_fetch_long(&lock->rlock.counter) & STU_SPINLOCK_OWNER_MASK); ) {
		now = time(NULL);

		if (now - start > 5) {
			//stu_log_error(0, "stu_sched_yield()");
			stu_sched_yield();
		}

		stu_cpu_pause();
	}
}

void
stu_spin_unlock(stu_spinlock_t *lock) {
	stu_atomic_fetch_add(&lock->rlock.counter, 1);
}

