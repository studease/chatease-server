/*
 * stu_list.c
 *
 *  Created on: 2016-10-18
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_list_t *
stu_list_create(stu_slab_pool_t *pool) {
	stu_list_t *list;

	list = stu_slab_alloc(pool, sizeof(stu_list_t));
	if (list == NULL) {
		stu_log_error(0, "Failed to create list.");
		return NULL;
	}

	list->pool = pool;
	stu_list_init(list, pool);

	return list;
}

void
stu_list_init(stu_list_t *list, stu_slab_pool_t *pool) {
	stu_spinlock_init(&list->lock);

	stu_queue_init(&list->elts.queue);
	list->elts.obj = NULL;
	list->elts.size = 0;

	list->pool = pool;
}

void
stu_list_destroy(stu_list_t *list) {
	stu_list_elt_t *elt;
	stu_queue_t    *q;

	q = stu_queue_head(&list->elts.queue);
	for (; q != stu_queue_sentinel(&list->elts.queue); q = q->next) {
		elt = stu_queue_data(q, stu_list_elt_t, queue);
		stu_slab_free(list->pool, (void *) elt);
	}

	stu_slab_free(list->pool, (void *) list);
}


stu_list_elt_t *
stu_list_push(stu_list_t *list, void *obj, size_t size) {
	stu_list_elt_t *elt;

	elt = stu_slab_alloc(list->pool, sizeof(stu_list_elt_t));
	if (elt == NULL) {
		stu_log_error(0, "Failed to alloc list elt.");
		return NULL;
	}

	elt->obj = obj;
	elt->size = size;

	stu_queue_insert_tail(&list->elts.queue, &elt->queue);

	return elt;
}

void
stu_list_remove(stu_list_t *list, stu_list_elt_t *elt) {
	stu_queue_remove(&elt->queue);

	stu_slab_free(list->pool, (void *) elt);
}

