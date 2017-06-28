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

#if (__WORDSIZE == 64)
#define STU_TIMER_MAXIMUM     512
#else
#define STU_TIMER_MAXIMUM     1024
#endif

#define STU_TIMER_INFINITE   (stu_msec_t) -1
#define STU_TIMER_LAZY_DELAY  300

/* used in stu_log_debug() */
#define stu_timer_ident(p)   ((stu_connection_t *) (p))->fd

stu_int_t  stu_timer_init(stu_cycle_t *cycle);
stu_msec_t stu_timer_find(void);
void stu_timer_expire(void);
void stu_timer_cancel(void);

stu_inline void stu_timer_add(stu_event_t *ev, stu_msec_t timer);
void            stu_timer_add_locked(stu_event_t *ev, stu_msec_t timer);

stu_inline void stu_timer_del(stu_event_t *ev);
void            stu_timer_del_locked(stu_event_t *ev);

#endif /* STU_TIMER_H_ */
