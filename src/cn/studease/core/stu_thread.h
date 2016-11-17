/*
 * stu_thread.h
 *
 *  Created on: 2016-9-14
 *      Author: Tony Lau
 */

#ifndef STU_THREAD_H_
#define STU_THREAD_H_

#include <stddef.h>
#include <pthread.h>
#include "stu_config.h"
#include "stu_core.h"

#define STU_THREADS_MAXIMUM  64

typedef pthread_t        stu_tid_t;
typedef pthread_key_t    stu_thread_key_t;
typedef pthread_cond_t   stu_cond_t;

#define stu_thread_key_create(key)  pthread_key_create(key, NULL)

typedef struct {
	stu_tid_t   id;
	stu_cond_t  cond;
	stu_uint_t  state;
} stu_thread_t;

typedef void *  stu_thread_value_t;

stu_int_t stu_init_threads(int n, size_t size);
stu_int_t stu_create_thread(stu_tid_t *tid, stu_thread_value_t (*func)(void *arg), void *arg);

stu_int_t stu_cond_init(stu_cond_t *cond);

#endif /* STU_THREAD_H_ */
