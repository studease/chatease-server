/*
 * stu_event.h
 *
 *  Created on: 2016-9-22
 *      Author: Tony Lau
 */

#ifndef STU_EVENT_H_
#define STU_EVENT_H_

#include <sys/epoll.h>
#include "stu_config.h"
#include "stu_core.h"

#define STU_EPOLL_SIZE   4096
#define STU_EPOLL_EVENTS 8

#define STU_READ_EVENT   (EPOLLIN|EPOLLRDHUP)
#define STU_WRITE_EVENT  EPOLLOUT
#define STU_CLOSE_EVENT  0x4000

typedef struct stu_event_s stu_event_t;
typedef void (*stu_event_handler_pt)(stu_event_t *ev);


struct stu_event_s {
	stu_bool_t            active;

	void                 *data;
	uint32_t              type;
	stu_event_handler_pt  handler;
};

stu_int_t stu_epoll_init();

stu_int_t stu_epoll_add_event(stu_event_t *ev, uint32_t event);
stu_int_t stu_epoll_del_event(stu_event_t *ev, uint32_t event);

stu_int_t stu_epoll_process_events(struct epoll_event *events, int maxevents, int timeout);

#endif /* STU_EVENT_H_ */
