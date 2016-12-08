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
	volatile stu_uint_t  counter;
} stu_atomic_t;


//#define stu_memory_barrier() __asm__ __volatile__ ("": : :"memory")
#define stu_memory_barrier() __sync_synchronize()

#define stu_atomic_release(lock)                   \
	__sync_lock_release(&(lock)->counter)

#define stu_atomic_test_set(lock, set)             \
	__sync_lock_test_and_set(&(lock)->counter, set)

#define stu_atomic_cmp_set(lock, old, set)         \
	__sync_bool_compare_and_swap(&(lock)->counter, old, set)

#define stu_atomic_fetch_add(lock, add)            \
	__sync_fetch_and_add(&(lock)->counter, add)

/*
static inline void
stu_atomic_set(stu_atomic_t *v, stu_int_t i) {
	stu_atomic_test_set(v, i);
	//v->counter = i;
}

static inline stu_int_t
stu_atomic_add(stu_int_t i, stu_atomic_t *v) {
	stu_int_t  c;

	//asm volatile(STU_LOCK_PREFIX "addl %k2,%k0"
	//		 : "+m" (v->counter), "=qm" (c)
	//		 : "ir" (i) : "memory");

	c = v->counter;
	v->counter += i;

	return c;
}
*/

static inline stu_int_t
stu_atomic_read(const stu_atomic_t *v) {
	return (*(volatile stu_uint_t *)&(v)->counter);
}

#endif /* STU_ATOMIC_H_ */
