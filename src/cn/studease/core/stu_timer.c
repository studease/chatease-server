/*
 * stu_timer.c
 *
 *  Created on: 2017-6-21
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

extern stu_cycle_t *stu_cycle;

/*
 * the event timer rbtree may contain the duplicate keys, however,
 * it should not be a problem, because we use the rbtree to find
 * a minimum timer value only
 */

stu_int_t
stu_timer_init(stu_cycle_t *cycle) {
	stu_spinlock_init(&cycle->timer_lock);
	stu_rbtree_init(&cycle->timer_rbtree, &cycle->timer_sentinel, stu_rbtree_insert_timer_value);

	return STU_OK;
}

stu_msec_t
stu_timer_find(void) {
	stu_msec_int_t     timer;
	stu_rbtree_node_t *node, *root, *sentinel;

	stu_spin_lock(&stu_cycle->timer_lock);

	if (stu_cycle->timer_rbtree.root == &stu_cycle->timer_sentinel) {
		timer = STU_TIMER_INFINITE;
		goto done;
	}

	root = stu_cycle->timer_rbtree.root;
	sentinel = stu_cycle->timer_rbtree.sentinel;
	node = stu_rbtree_min(root, sentinel);

	timer = (stu_msec_int_t) (node->key - stu_current_msec);
	timer = timer > 0 ? timer : 0;

done:

	stu_spin_unlock(&stu_cycle->timer_lock);

	return (stu_msec_t) timer;
}

void
stu_timer_expire(void) {
	stu_event_t       *ev;
	stu_rbtree_node_t *node, *root, *sentinel;

	stu_spin_lock(&stu_cycle->timer_lock);

	sentinel = stu_cycle->timer_rbtree.sentinel;

	for ( ;; ) {
		root = stu_cycle->timer_rbtree.root;
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

		stu_rbtree_delete(&stu_cycle->timer_rbtree, &ev->timer);

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

	stu_spin_unlock(&stu_cycle->timer_lock);
}

void
stu_timer_cancel(void) {
	stu_event_t        *ev;
	stu_rbtree_node_t  *node, *root, *sentinel;

	stu_spin_lock(&stu_cycle->timer_lock);

	sentinel = stu_cycle->timer_rbtree.sentinel;

	for ( ;; ) {
		root = stu_cycle->timer_rbtree.root;
		if (root == sentinel) {
			goto done;
		}

		node = stu_rbtree_min(root, sentinel);

		ev = (stu_event_t *) ((char *) node - offsetof(stu_event_t, timer));
		if (!ev->cancelable) {
			goto done;
		}

		stu_log_debug(3, "timer cancel: fd=%d, key=%lu.", stu_timer_ident(ev->data), ev->timer.key);

		stu_rbtree_delete(&stu_cycle->timer_rbtree, &ev->timer);

#if (STU_DEBUG)
		ev->timer.left = NULL;
		ev->timer.right = NULL;
		ev->timer.parent = NULL;
#endif

		ev->timer_set = 0;

		ev->handler(ev);
	}

done:

	stu_spin_unlock(&stu_cycle->timer_lock);
}


stu_inline void
stu_timer_add(stu_event_t *ev, stu_msec_t timer) {
	stu_spin_lock(&stu_cycle->timer_lock);
	stu_timer_add_locked(ev, timer);
	stu_spin_unlock(&stu_cycle->timer_lock);
}

void
stu_timer_add_locked(stu_event_t *ev, stu_msec_t timer) {
	stu_msec_t      key;
	stu_msec_int_t  diff;

	key = stu_current_msec + timer;

	if (ev->timer_set) {
		/*
		 * Use a previous timer value if difference between it and a new
		 * value is less than STU_TIMER_LAZY_DELAY milliseconds: this allows
		 * to minimize the rbtree operations for fast connections.
		 */

		diff = (stu_msec_int_t) (key - ev->timer.key);
		if (stu_abs(diff) < STU_TIMER_LAZY_DELAY) {
			stu_log_debug(3, "timer: %d, old: %lu, new: %lu.", stu_timer_ident(ev->data), ev->timer.key, key);
			return;
		}

		stu_timer_del_locked(ev);
	}

	ev->timer.key = key;
	stu_log_debug(3, "timer add: fd=%d, %lu:%lu.", stu_timer_ident(ev->data), timer, ev->timer.key);

	stu_rbtree_insert(&stu_cycle->timer_rbtree, &ev->timer);

	ev->timer_set = 1;
}

stu_inline void
stu_timer_del(stu_event_t *ev) {
	stu_spin_lock(&stu_cycle->timer_lock);
	stu_timer_del_locked(ev);
	stu_spin_unlock(&stu_cycle->timer_lock);
}

void
stu_timer_del_locked(stu_event_t *ev) {
	stu_rbtree_delete(&stu_cycle->timer_rbtree, &ev->timer);
	stu_log_debug(3, "timer delete: fd=%d, key=%lu.", stu_timer_ident(ev->data), ev->timer.key);

#if (STU_DEBUG)
	ev->timer.left = NULL;
	ev->timer.right = NULL;
	ev->timer.parent = NULL;
#endif

	ev->timer_set = 0;
}
