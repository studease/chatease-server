/*
 * stu_websocket_request.c
 *
 *  Created on: 2016-11-24
 *      Author: Tony Lau
 */

#include <sys/socket.h>
#include "stu_config.h"
#include "stu_core.h"

void
stu_websocket_wait_request_handler(stu_event_t *rev) {
	stu_connection_t *c;
	u_char            buf[2048];
	stu_int_t         n;

	c = (stu_connection_t *) rev->data;
	stu_memzero(buf, 2048);

	n = recv(c->fd, buf, 2048, 0);
	if (n == -1) {
		if (stu_errno == EAGAIN) {
			return;
		}

		stu_log_debug(0, "Failed to recv data: fd=%d.", c->fd);
		stu_connection_free(c);
		return;
	}

	if (n == 0) {
		stu_log_debug(0, "Remote client has closed connection.");
		stu_websocket_close_connection(c);
		return;
	}

	c->data = (void *) stu_websocket_create_request(c);
	if (c->data == NULL) {
		stu_log_error(0, "Failed to create websocket request.");
		return;
	}

	stu_websocket_process_request((stu_websocket_request_t *) c->data);
}

stu_websocket_request_t *
stu_websocket_create_request(stu_connection_t *c) {
	stu_websocket_request_t *r;
	stu_int_t                size;

	size = sizeof(stu_websocket_request_t);

	stu_spin_lock(&c->pool->lock);

	if (c->pool->data.end - c->pool->data.last < size) {
		stu_log_error(0, "Failed to alloc websocket request.");
		return NULL;
	}

	r = (stu_websocket_request_t *) c->pool->data.last;
	c->pool->data.last += size;

	stu_spin_unlock(&c->pool->lock);

	r->connection = c;

	return r;
}

void
stu_websocket_process_request(stu_websocket_request_t *r) {

}

void
stu_websocket_request_handler(stu_event_t *wev) {

}


void
stu_websocket_close_request(stu_websocket_request_t *r, stu_int_t rc) {
	stu_connection_t *c;

	c = r->connection;

	stu_websocket_free_request(r, rc);
	stu_websocket_close_connection(c);
}

void
stu_websocket_free_request(stu_websocket_request_t *r, stu_int_t rc) {
	r->connection->data = NULL;
}

void
stu_websocket_close_connection(stu_connection_t *c) {
	stu_connection_close(c);
}


void
stu_websocket_empty_handler(stu_event_t *wev) {

}

void
stu_websocket_request_empty_handler(stu_websocket_request_t *r) {

}

