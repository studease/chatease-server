/*
 * stu_queue.h
 *
 *  Created on: 2016-9-13
 *      Author: Tony Lau
 */

#ifndef STU_QUEUE_H_
#define STU_QUEUE_H_

#include "stu_config.h"
#include "stu_core.h"

typedef struct stu_queue_s  stu_queue_t;

struct stu_queue_s {
	stu_queue_t  *prev;
	stu_queue_t  *next;
};

#define stu_queue_init(q)                                                    \
	(q)->prev = q;                                                            \
	(q)->next = q


#define stu_queue_empty(h)                                                   \
	(h == (h)->prev)


#define stu_queue_insert_head(h, x)                                          \
	(x)->next = (h)->next;                                                    \
	(x)->next->prev = x;                                                      \
	(x)->prev = h;                                                            \
	(h)->next = x


#define stu_queue_insert_after   stu_queue_insert_head


#define stu_queue_insert_tail(h, x)                                          \
	(x)->prev = (h)->prev;                                                    \
	(x)->prev->next = x;                                                      \
	(x)->next = h;                                                            \
	(h)->prev = x


#define stu_queue_head(h)                                                    \
	(h)->next


#define stu_queue_last(h)                                                    \
	(h)->prev


#define stu_queue_sentinel(h)                                                \
	(h)


#define stu_queue_next(q)                                                    \
	(q)->next


#define stu_queue_prev(q)                                                    \
	(q)->prev


#define stu_queue_remove(x)                                                  \
	(x)->next->prev = (x)->prev;                                              \
	(x)->prev->next = (x)->next;                                              \
	(x)->prev = NULL;                                                         \
	(x)->next = NULL


#define stu_queue_split(h, q, n)                                             \
	(n)->prev = (h)->prev;                                                    \
	(n)->prev->next = n;                                                      \
	(n)->next = q;                                                            \
	(h)->prev = (q)->prev;                                                    \
	(h)->prev->next = h;                                                      \
	(q)->prev = n;


#define stu_queue_add(h, n)                                                  \
	(h)->prev->next = (n)->next;                                              \
	(n)->next->prev = (h)->prev;                                              \
	(h)->prev = (n)->prev;                                                    \
	(h)->prev->next = h;


#define stu_queue_data(q, type, link)                                        \
	(type *) ((u_char *) q - offsetof(type, link))


#endif /* STU_QUEUE_H_ */
