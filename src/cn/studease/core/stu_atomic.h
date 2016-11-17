/*
 * stu_atomic.h
 *
 *  Created on: 2016-10-10
 *      Author: Tony Lau
 */

#ifndef STU_ATOMIC_H_
#define STU_ATOMIC_H_

#include "stu_config.h"
#include "stu_core.h"

#if (CONFIG_SMP)
#define STU_LOCK_PREFIX  "lock;"
#else
#define STU_LOCK_PREFIX
#endif

typedef struct {
	stu_int_t  counter;
} stu_atomic_t;


static inline void
stu_atomic_set(stu_atomic_t *v, stu_int_t i) {
	v->counter = i;
}

static inline stu_int_t
stu_atomic_add(stu_int_t i, stu_atomic_t *v) {
	stu_int_t  c;

	/*asm volatile(STU_LOCK_PREFIX "addl %k2,%k0"
			 : "+m" (v->counter), "=qm" (c)
			 : "ir" (i) : "memory");*/

	c = v->counter;
	v->counter += i;

	return c;
}

static inline stu_int_t
stu_atomic_read(const stu_atomic_t *v) {
	return (*(volatile int *)&(v)->counter);
}

#endif /* STU_ATOMIC_H_ */
