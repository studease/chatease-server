/*
 * stu_list.c
 *
 *  Created on: 2016-10-18
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

void
stu_list_init(stu_list_t *list, void *pool, stu_list_palloc_pt palloc, stu_list_free_pt free) {
	stu_spinlock_init(&list->lock);

	stu_queue_init(&list->elts.queue);
	list->elts.obj = NULL;
	list->elts.size = 0;

	list->length = 0;

	list->pool = pool;
	list->palloc = palloc;
	list->free = free;
}

void
stu_list_destroy(stu_list_t *list) {

}


stu_int_t
stu_list_push(stu_list_t *list, void *obj, size_t size) {
	stu_list_elt_t *elt;

	elt = list->palloc(list->pool, sizeof(stu_list_elt_t));
	if (elt == NULL) {
		stu_log_error(0, "Failed to palloc stu_list_elt_t.");
		return STU_ERROR;
	}

	elt->obj = obj;
	elt->size = size;

	stu_queue_insert_tail(&list->elts.queue, &elt->queue);
	list->length++;

	return STU_OK;
}

void
stu_list_remove(stu_list_t *list, stu_list_elt_t *elt) {
	stu_queue_remove(&elt->queue);
	list->length--;

	if (list->free) {
		list->free(list->pool, elt);
	}
}

void
stu_list_foreach(stu_list_t *list, stu_list_foreach_pt cb) {

}

