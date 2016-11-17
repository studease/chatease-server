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

#define STU_SPINLOCK_OWNER_MASK 0X0000FFFF

typedef struct {
	stu_atomic_t  rlock;
} stu_spinlock_t;


void stu_spinlock_init(stu_spinlock_t *lock);

void stu_spin_lock(stu_spinlock_t *lock);
void stu_spin_unlock(stu_spinlock_t *lock);

#endif /* STU_SPINLOCK_H_ */
