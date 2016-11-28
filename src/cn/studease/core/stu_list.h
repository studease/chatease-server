/*
 * stu_list.h
 *
 *  Created on: 2016-10-17
 *      Author: root
 */

#ifndef STU_LIST_H_
#define STU_LIST_H_

#include "stu_config.h"
#include "stu_core.h"

typedef struct {
	stu_queue_t      queue;

	void            *obj;
	size_t           size;
} stu_list_elt_t;

typedef struct {
	stu_spinlock_t   lock;

	stu_slab_pool_t *pool;
	stu_list_elt_t   elts;
} stu_list_t;


stu_list_t *stu_list_create(stu_slab_pool_t *pool);
void stu_list_init(stu_list_t *list, stu_slab_pool_t *pool);
void stu_list_destroy(stu_list_t *list);

void stu_list_push(stu_list_t *list, stu_list_elt_t *elt);
void stu_list_remove(stu_list_t *list, stu_list_elt_t *elt);

#endif /* STU_LIST_H_ */
