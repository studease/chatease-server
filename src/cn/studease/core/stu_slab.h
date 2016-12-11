/*
 * stu_slab.h
 *
 *  Created on: 2016-9-9
 *      Author: Tony Lau
 */

#ifndef STU_SLAB_H_
#define STU_SLAB_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_SLAB_PAGE_SIZE  4096
#define STU_SLAB_PAGE_SHIFT 12

#define STU_SLAB_MIN_SIZE   8
#define STU_SLAB_MIN_SHIFT  3

#define STU_SLAB_MID_SIZE   64
#define STU_SLAB_MID_SHIFT  6

#define STU_SLAB_BIG_SIZE   512
#define STU_SLAB_BIG_SHIFT  9

#define STU_SLAB_MAX_SIZE   2048

#define STU_SLAB_BUSY64     0xFFFFFFFFFFFFFFFF
#define STU_SLAB_BUSY8      0xFF

#define STU_SLAB_POOL_DEFAULT_SIZE  8388608


typedef struct stu_slab_page_s stu_slab_page_t;

struct stu_slab_page_s {
	stu_uint_t       shift;
	uintptr_t        bitmap;

	stu_slab_page_t *prev;
	stu_slab_page_t *next;
};

typedef struct {
	stu_spinlock_t   lock;
	stu_pool_data_t  data;

	stu_slab_page_t *slots;
	stu_slab_page_t *pages;
	stu_slab_page_t *last;
	stu_slab_page_t  free;
} stu_slab_pool_t;

stu_slab_pool_t *stu_slab_pool_create(size_t size);

void *stu_slab_alloc(stu_slab_pool_t *pool, size_t size);
void *stu_slab_alloc_locked(stu_slab_pool_t *pool, size_t size);

void *stu_slab_calloc(stu_slab_pool_t *pool, size_t size);
void *stu_slab_calloc_locked(stu_slab_pool_t *pool, size_t size);

void  stu_slab_free(stu_slab_pool_t *pool, void *p);
void  stu_slab_free_locked(stu_slab_pool_t *pool, void *p);

#endif /* STU_SLAB_H_ */
