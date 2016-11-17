/*
 * stu_palloc.h
 *
 *  Created on: 2016-9-12
 *      Author: Tony Lau
 */

#ifndef STU_PALLOC_H_
#define STU_PALLOC_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_POOL_DEFAULT_SIZE   (16 * 1024)
#define STU_POOL_MAX_ALLOC_SIZE 2048

typedef struct stu_base_pool_s stu_base_pool_t;

typedef struct {
	u_char           *start;
	u_char           *last;
	u_char           *end;
} stu_pool_data_t;

struct stu_base_pool_s {
	stu_spinlock_t   lock;
	stu_pool_data_t  data;

	stu_base_pool_t *prev;
	stu_base_pool_t *next;
};

struct stu_pool_s {
	stu_spinlock_t   lock;
	stu_pool_data_t  data;

	stu_pool_t      *prev;
	stu_pool_t      *next;
	stu_pool_t      *last;

	stu_uint_t       max;
	stu_base_pool_t *large;
	stu_uint_t       failed;
};

stu_pool_t *stu_pool_create(size_t size);
void stu_pool_destroy(stu_pool_t *pool);
void stu_pool_reset(stu_pool_t *pool);

void *stu_palloc(stu_pool_t *pool, size_t size);
void *stu_pcalloc(stu_pool_t *pool, size_t size);

#endif /* STU_PALLOC_H_ */
