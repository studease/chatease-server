/*
 * stu_event_epoll.c
 *
 *  Created on: 2017-6-22
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static int  stu_epfd = -1;


stu_int_t
stu_event_epoll_init() {
	if (stu_epfd == -1) {
		stu_epfd = epoll_create(STU_EPOLL_SIZE);
		if (stu_epfd == -1) {
			stu_log_error(stu_errno, "Failed to create epoll.");
			return STU_ERROR;
		}
	}

	return STU_OK;
}

stu_int_t
stu_event_epoll_add(stu_event_t *ev, uint32_t event, stu_uint_t flags) {
	stu_connection_t   *c;
	int                 op;
	struct epoll_event  ee;

	c = (stu_connection_t *) ev->data;

	if ((ev->type & event) == event) {
		goto done;
	}

	ev->type |= event | (uint32_t) flags;
	op = ev->active ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

	ee.events = ev->type;
	ee.data.ptr = (void *) c;

	stu_log_debug(3, "epoll add event: fd=%d, op=%d, ev=%X.", c->fd, op, ee.events);

	if (epoll_ctl(stu_epfd, op, c->fd, &ee) == -1) {
		stu_log_error(stu_errno, "epoll_ctl(%d, %d) failed", op, c->fd);
		return STU_ERROR;
	}

done:

	ev->active = 1;

	return STU_OK;
}

stu_int_t
stu_event_epoll_del(stu_event_t *ev, uint32_t event, stu_uint_t flags) {
	stu_connection_t   *c;
	int                 op;
	struct epoll_event  ee;

	/*
	 * when the file descriptor is closed, the epoll automatically deletes
	 * it from its queue, so we do not need to delete explicitly the event
	 * before closing the file descriptor
	 */
	if (ev->type == STU_CLOSE_EVENT) {
		ev->active = 0;
		return STU_OK;
	}

	c = (stu_connection_t *) ev->data;

	if ((ev->type & event) == 0) {
		goto done;
	}

	ev->type &= ~event;

	if (ev->active) {
		op = EPOLL_CTL_MOD;
		ee.events = ev->type | flags;
		ee.data.ptr = (void *) c;
	} else {
		op = EPOLL_CTL_DEL;
		ee.events = 0;
		ee.data.ptr = NULL;
	}

	stu_log_debug(3, "epoll del event: fd=%d, op=%d, ev=%X.", c->fd, op, ee.events);

	if (epoll_ctl(stu_epfd, op, c->fd, &ee) == -1) {
		stu_log_error(stu_errno, "epoll_ctl(%d, %d) failed", op, c->fd);
		return STU_ERROR;
	}

done:

	//ev->active = 0;

	return STU_OK;
}

stu_int_t
stu_event_epoll_process_events(stu_msec_t timer, stu_uint_t flags) {
	struct epoll_event  events[STU_EPOLL_EVENTS], *ev;
	stu_int_t           nev, i;
	stu_connection_t   *c;

	nev = epoll_wait(stu_epfd, events, STU_EPOLL_EVENTS, timer);

	if (flags & STU_EVENT_FLAGS_UPDATE_TIME) {
		stu_time_update();
	}

	if (nev == -1) {
		stu_log_error(stu_errno, "epoll_wait error: nev=%d.", nev);
		return STU_ERROR;
	}

	for (i = 0; i < nev; i++) {
		ev = &events[i];
		c = (stu_connection_t *) events[i].data.ptr;
		if (c == NULL || c->fd == (stu_socket_t) -1) {
			continue;
		}

		if ((ev->events & EPOLLIN) && c->read.active) {
			c->read.handler(&c->read);
		}

		if ((ev->events & EPOLLOUT) && c->write.active) {
			c->write.handler(&c->write);
		}
	}

	return STU_OK;
}
