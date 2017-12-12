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

typedef void (*stu_list_foreach_pt) (stu_str_t *key, void *value);

typedef void *(*stu_list_palloc_pt)(size_t size);
typedef void (*stu_list_free_pt)(void *p);

typedef struct {
	stu_queue_t  queue;

	void        *obj;
	size_t       size;
} stu_list_elt_t;

typedef struct {
	stu_mutex_t      lock;

	stu_list_elt_t      elts;
	stu_uint_t          length;

	stu_list_palloc_pt  palloc;
	stu_list_free_pt    free;
} stu_list_t;


void stu_list_init(stu_list_t *list, stu_list_palloc_pt palloc, stu_list_free_pt free);
void stu_list_destroy(stu_list_t *list);

stu_int_t stu_list_push(stu_list_t *list, void *obj, size_t size);
void stu_list_remove(stu_list_t *list, stu_list_elt_t *elt);

void stu_list_foreach(stu_list_t *list, stu_list_foreach_pt cb);

#endif /* STU_LIST_H_ */
