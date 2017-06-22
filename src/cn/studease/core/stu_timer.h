/*
 * stu_timer.h
 *
 *  Created on: 2017-6-21
 *      Author: Tony Lau
 */

#ifndef STU_TIMER_H_
#define STU_TIMER_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_TIMER_INFINITE   (stu_msec_t) -1
#define STU_TIMER_LAZY_DELAY  300

/* used in stu_log_debug() */
#define stu_timer_ident(p)   ((stu_connection_t *) (p))->fd

stu_int_t  stu_timer_init(void);
stu_msec_t stu_timer_find(void);
void stu_timer_expire(void);
void stu_timer_cancel(void);

extern stu_spinlock_t  stu_timer_lock;
extern stu_rbtree_t    stu_timer_rbtree;

static void stu_timer_del_locked(stu_event_t *ev);
static void stu_timer_add_locked(stu_event_t *ev, stu_msec_t timer);

static stu_inline void
stu_timer_del(stu_event_t *ev) {
	stu_spin_lock(&stu_timer_lock);
	stu_timer_del_locked(ev);
	stu_spin_unlock(&stu_timer_lock);
}

static void
stu_timer_del_locked(stu_event_t *ev) {
	stu_rbtree_delete(&stu_timer_rbtree, &ev->timer);
	stu_log_debug(3, "timer delete: fd=%d, key=%lu.", stu_timer_ident(ev->data), ev->timer.key);

#if (STU_DEBUG)
	ev->timer.left = NULL;
	ev->timer.right = NULL;
	ev->timer.parent = NULL;
#endif

	ev->timer_set = 0;
}

static stu_inline void
stu_timer_add(stu_event_t *ev, stu_msec_t timer) {
	stu_spin_lock(&stu_timer_lock);
	stu_timer_add_locked(ev, timer);
	stu_spin_unlock(&stu_timer_lock);
}

static void
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

	stu_rbtree_insert(&stu_timer_rbtree, &ev->timer);

	ev->timer_set = 1;
}

#endif /* STU_TIMER_H_ */
