/*
 * stu_event_epoll.h
 *
 *  Created on: 2017-6-22
 *      Author: Tony Lau
 */

#ifndef STU_EVENT_EPOLL_H_
#define STU_EVENT_EPOLL_H_

#include <sys/epoll.h>
#include "stu_config.h"
#include "stu_core.h"

#define STU_EPOLL_SIZE   4096
#define STU_EPOLL_EVENTS 8


stu_int_t stu_event_epoll_init();

stu_int_t stu_event_epoll_add(stu_event_t *ev, uint32_t event, stu_uint_t flags);
stu_int_t stu_event_epoll_del(stu_event_t *ev, uint32_t event, stu_uint_t flags);

stu_int_t stu_event_epoll_process_events(stu_msec_t timer, stu_uint_t flags);

#endif /* STU_EVENT_EPOLL_H_ */
