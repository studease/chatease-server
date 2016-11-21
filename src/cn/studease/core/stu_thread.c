/*
 * stu_thread.c
 *
 *  Created on: 2016-9-14
 *      Author: Tony Lau
 */

#include <pthread.h>
#include "stu_config.h"
#include "stu_core.h"

static pthread_attr_t  thr_attr;

//extern volatile stu_thread_t  stu_threads[STU_MAX_THREADS];
extern stu_int_t       stu_threads_n;


stu_int_t
stu_init_threads(int n, size_t size) {
	int  err;

	err = pthread_attr_init(&thr_attr);
	if (err != 0) {
		stu_log_error(err, "pthread_attr_init() failed");
		return STU_ERROR;
	}

	err = pthread_attr_setstacksize(&thr_attr, size);

	if (err != 0) {
		stu_log_error(err, "pthread_attr_setstacksize() failed");
		return STU_ERROR;
	}

	return STU_OK;
}

stu_int_t
stu_create_thread(stu_tid_t *tid, stu_thread_value_t (*func)(void *arg), void *arg) {
	int  err;

	if (stu_threads_n >= STU_THREADS_MAXIMUM) {
		stu_log_error(0, "no more than %d threads can be created", STU_THREADS_MAXIMUM);
		return STU_ERROR;
	}

	err = pthread_create(tid, &thr_attr, func, arg);
	if (err != 0) {
		stu_log_error(err, "pthread_create() failed");
		return STU_ERROR;
	}

	stu_log_debug(0, "thread %p is created.", tid);
	stu_threads_n++;

	return STU_OK;
}

stu_int_t
stu_cond_init(stu_cond_t *cond) {
	stu_int_t  err;

	err = pthread_cond_init(cond, NULL);
	if (err != STU_OK) {
		stu_log_error(err, "pthread_cond_init() failed");
		return STU_ERROR;
	}

	return STU_OK;
}

