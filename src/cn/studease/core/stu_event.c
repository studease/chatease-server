/*
 * stu_event.c
 *
 *  Created on: 2016-9-22
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static int  stu_epfd = -1;

stu_int_t
stu_epoll_init() {
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
stu_epoll_add_event(stu_event_t *ev, uint32_t event) {
	stu_connection_t   *c;
	int                 op;
	struct epoll_event  ee;

	c = (stu_connection_t *) ev->data;

	if ((ev->type & event) == event) {
		goto done;
	}

	ev->type |= event;
	op = ev->active ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

	ee.events = ev->type;
	ee.data.ptr = (void *) c;

	stu_log_debug(0, "epoll add event: fd=%d, op=%d, ev=%d.", c->fd, op, ee.events);

	if (epoll_ctl(stu_epfd, op, c->fd, &ee) == -1) {
		stu_log_error(stu_errno, "epoll_ctl(%d, %d) failed", op, c->fd);
		return STU_ERROR;
	}

done:

	ev->active = TRUE;

	return STU_OK;
}

stu_int_t
stu_epoll_del_event(stu_event_t *ev, uint32_t event) {
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
		ee.events = ev->type;
		ee.data.ptr = (void *) c;
	} else {
		op = EPOLL_CTL_DEL;
		ee.events = 0;
		ee.data.ptr = NULL;
	}

	stu_log_debug(0, "epoll del event: fd=%d, op=%d, ev=%d.", c->fd, op, ee.events);

	if (epoll_ctl(stu_epfd, op, c->fd, &ee) == -1) {
		stu_log_error(stu_errno, "epoll_ctl(%d, %d) failed", op, c->fd);
		return STU_ERROR;
	}

done:

	ev->active = 0;

	return STU_OK;
}

stu_int_t
stu_epoll_process_events(struct epoll_event *events, int maxevents, int timeout) {
	stu_int_t  nev;

	nev = epoll_wait(stu_epfd, events, maxevents, timeout);

	return nev;
}

