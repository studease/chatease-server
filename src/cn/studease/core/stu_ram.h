/*
 * stu_ram.h
 *
 *  Created on: 2016-10-28
 *      Author: Tony Lau
 */

#ifndef STU_RAM_H_
#define STU_RAM_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_RAM_BLOCK_SIZE  4096
#define STU_RAM_BLOCK_SHIFT 12

#define STU_RAM_PAGE_SIZE   (512 * STU_RAM_BLOCK_SIZE)
#define STU_RAM_PAGE_SHIFT  21

#define STU_RAM_POOL_BUSY   0xFF
#define STU_RAM_PAGE_BUSY   0xFF
#define STU_RAM_BUSY64      0xFFFFFFFFFFFFFFFF
#define STU_RAM_BUSY8       0xFF


typedef struct stu_ram_page_s stu_ram_page_t;

struct stu_ram_page_s {
	uintptr_t        bitmap;

	stu_ram_page_t  *prev;
	stu_ram_page_t  *next;
};

typedef struct {
	stu_queue_t      queue;
	stu_spinlock_t   lock;
	stu_pool_data_t  data;

	uint64_t         bitmap;

	stu_ram_page_t  *slots;
	stu_ram_page_t  *pages;
	stu_ram_page_t  *last;
	stu_ram_page_t   free;
} stu_ram_pool_t;

stu_ram_pool_t *stu_ram_pool_create();

void *stu_ram_alloc(stu_ram_pool_t *pool);
void *stu_ram_alloc_locked(stu_ram_pool_t *pool);

void  stu_ram_free(stu_ram_pool_t *pool, void *p);
void  stu_ram_free_locked(stu_ram_pool_t *pool, void *p);

#endif /* STU_RAM_H_ */
