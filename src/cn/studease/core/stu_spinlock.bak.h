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

#define stu_spinlock_t  spinlock_t

#define stu_spinlock_init(lock)   \
	spin_lock_init(lock)

#define stu_spin_lock(lock)       \
	spin_lock_irq(lock)

#define stu_spin_unlock(lock)     \
	spin_unlock_irq(lock)

stu_int_t
stu_spin_is_locked(stu_spinlock_t *lock) {
	return spin_is_locked(lock);
}

stu_int_t
stu_spin_can_lock(stu_spinlock_t *lock) {
	return spin_can_lock(lock);
}

stu_int_t
stu_spin_trylock(stu_spinlock_t *lock) {
	return spin_trylock_irq(lock);
}

#endif /* STU_SPINLOCK_H_ */
