/*
 * stu_timer.c
 *
 *  Created on: 2017-6-21
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_spinlock_t            stu_timer_lock;
stu_rbtree_t              stu_timer_rbtree;
static stu_rbtree_node_t  stu_timer_sentinel;

/*
 * the event timer rbtree may contain the duplicate keys, however,
 * it should not be a problem, because we use the rbtree to find
 * a minimum timer value only
 */

stu_int_t
stu_timer_init(void) {
	stu_spinlock_init(&stu_timer_lock);
	stu_rbtree_init(&stu_timer_rbtree, &stu_timer_sentinel, stu_rbtree_insert_timer_value);

	return STU_OK;
}

stu_msec_t
stu_timer_find(void) {
	stu_msec_int_t     timer;
	stu_rbtree_node_t *node, *root, *sentinel;

	stu_spin_lock(&stu_timer_lock);

	if (stu_timer_rbtree.root == &stu_timer_sentinel) {
		timer = STU_TIMER_INFINITE;
		goto done;
	}

	root = stu_timer_rbtree.root;
	sentinel = stu_timer_rbtree.sentinel;
	node = stu_rbtree_min(root, sentinel);

	timer = (stu_msec_int_t) (node->key - stu_current_msec);
	timer = timer > 0 ? timer : 0;

done:

	stu_spin_unlock(&stu_timer_lock);

	return (stu_msec_t) timer;
}

void
stu_timer_expire(void) {
	stu_event_t       *ev;
	stu_rbtree_node_t *node, *root, *sentinel;

	stu_spin_lock(&stu_timer_lock);

	sentinel = stu_timer_rbtree.sentinel;

	for ( ;; ) {
		root = stu_timer_rbtree.root;
		if (root == sentinel) {
			goto done;
		}

		node = stu_rbtree_min(root, sentinel);

		/* node->key > stu_current_time */
		if ((stu_msec_int_t) (node->key - stu_current_msec) > 0) {
			goto done;
		}

		ev = (stu_event_t *) ((char *) node - offsetof(stu_event_t, timer));
		stu_log_debug(3, "timer delete: fd=%d, key=%lu.", stu_timer_ident(ev->data), ev->timer.key);

		stu_rbtree_delete(&stu_timer_rbtree, &ev->timer);

#if (STU_DEBUG)
		ev->timer.left = NULL;
		ev->timer.right = NULL;
		ev->timer.parent = NULL;
#endif

		ev->timedout = 1;
		ev->timer_set = 0;

		ev->handler(ev);
	}

done:

	stu_spin_unlock(&stu_timer_lock);
}

void
stu_timer_cancel(void) {
	stu_event_t        *ev;
	stu_rbtree_node_t  *node, *root, *sentinel;

	stu_spin_lock(&stu_timer_lock);

	sentinel = stu_timer_rbtree.sentinel;

	for ( ;; ) {
		root = stu_timer_rbtree.root;
		if (root == sentinel) {
			goto done;
		}

		node = stu_rbtree_min(root, sentinel);

		ev = (stu_event_t *) ((char *) node - offsetof(stu_event_t, timer));
		if (!ev->cancelable) {
			goto done;
		}

		stu_log_debug(3, "timer cancel: fd=%d, key=%lu.", stu_timer_ident(ev->data), ev->timer.key);

		stu_rbtree_delete(&stu_timer_rbtree, &ev->timer);

#if (STU_DEBUG)
		ev->timer.left = NULL;
		ev->timer.right = NULL;
		ev->timer.parent = NULL;
#endif

		ev->timer_set = 0;

		ev->handler(ev);
	}

done:

	stu_spin_unlock(&stu_timer_lock);
}
