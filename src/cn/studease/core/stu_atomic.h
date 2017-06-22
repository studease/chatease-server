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
	volatile uint32_t  counter;
} stu_atomic_t;


//#define stu_memory_barrier() __asm__ __volatile__ ("": : :"memory")
#define stu_memory_barrier() __sync_synchronize()

#if ( __i386__ || __i386 || __amd64__ || __amd64 )
#define stu_cpu_pause()      __asm__ ("pause")
#else
#define stu_cpu_pause()
#endif

#define stu_atomic_release(ptr)                   \
	__sync_lock_release(ptr)

#define stu_atomic_test_set(ptr, val)             \
	__sync_lock_test_and_set(ptr, val)

#define stu_atomic_cmp_set(ptr, old, val)         \
	__sync_bool_compare_and_swap(ptr, old, val)

#define stu_atomic_fetch_add(ptr, val)            \
	__sync_fetch_and_add(ptr, val)

#define stu_atomic_fetch_sub(ptr, val)            \
	__sync_fetch_and_sub(ptr, val)

#define stu_atomic_fetch(ptr)                     \
	stu_atomic_read((const uint32_t *) ptr)

#define stu_atomic_fetch_char(ptr)                \
	stu_atomic_read_char((const char *) ptr)

static inline uint32_t
stu_atomic_read(const uint32_t *v) {
	return (*(volatile uint32_t *)v);
}

static stu_inline char
stu_atomic_read_char(const char *v) {
	return (*(volatile char *)v);
}

#endif /* STU_ATOMIC_H_ */
