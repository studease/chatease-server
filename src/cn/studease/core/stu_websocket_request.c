/*
 * stu_websocket_request.c
 *
 *  Created on: 2016-11-24
 *      Author: Tony Lau
 */

#include <sys/socket.h>
#include "stu_config.h"
#include "stu_core.h"

static stu_int_t stu_websocket_process_request_frames(stu_websocket_request_t *r);


void
stu_websocket_wait_request_handler(stu_event_t *rev) {
	stu_connection_t *c;
	stu_int_t         n, err;

	c = (stu_connection_t *) rev->data;

	stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) -1) {
		goto done;
	}

	if (c->buffer.start == NULL) {
		c->buffer.start = (u_char *) stu_base_palloc(c->pool, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE);
		c->buffer.end = c->buffer.start + STU_WEBSOCKET_REQUEST_DEFAULT_SIZE;
	}
	c->buffer.last = c->buffer.start;
	stu_memzero(c->buffer.start, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE);

	n = recv(c->fd, c->buffer.start, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE, 0);
	if (n == -1) {
		err = stu_errno;
		if (err == EAGAIN) {
			goto done;
		}

		stu_log_error(err, "Failed to recv data: fd=%d.", c->fd);
		stu_connection_free(c);
		goto done;
	}

	if (n == 0) {
		stu_log_debug(0, "Remote client has closed connection: fd=%d.", c->fd);
		goto failed;
	}

	stu_log_debug(0, "recv: fd=%d, bytes=%d, str=\n%s", c->fd, n, c->buffer.start);

	c->data = (void *) stu_websocket_create_request(c);
	if (c->data == NULL) {
		stu_log_error(0, "Failed to create websocket request.");
		goto failed;
	}

	stu_websocket_process_request(c->data);

	goto done;

failed:

	c->read->active = FALSE;
	stu_epoll_del_event(c->read, STU_READ_EVENT);

	stu_websocket_close_connection(c);

done:

	stu_spin_unlock(&c->lock);
}

stu_websocket_request_t *
stu_websocket_create_request(stu_connection_t *c) {
	stu_websocket_request_t *r;

	if (c->data == NULL) {
		stu_spin_lock(&c->pool->lock);
		r = stu_base_pcalloc(c->pool, sizeof(stu_websocket_request_t));
		stu_spin_unlock(&c->pool->lock);
	} else {
		r = c->data;
	}

	r->connection = c;

	return r;
}

void
stu_websocket_process_request(stu_websocket_request_t *r) {
	stu_int_t  rc;

	rc = stu_websocket_process_request_frames(r);
	if (rc != STU_OK) {
		if (rc < 0) {
			rc = STU_HTTP_BAD_REQUEST;
		}

		stu_log_error(0, "Failed to process request frames.");
		stu_websocket_finalize_request(r, rc);
		return;
	}

	switch (r->command) {
	case STU_WEBSOCKET_CMD_JOIN:
		break;
	case STU_WEBSOCKET_CMD_LEFT:
		break;
	case STU_WEBSOCKET_CMD_UNI_DATA:
		break;
	case STU_WEBSOCKET_CMD_DATA:
		break;

	case STU_WEBSOCKET_CMD_MUTE:
		break;
	case STU_WEBSOCKET_CMD_UNMUTE:
		break;
	case STU_WEBSOCKET_CMD_FORBID:
		break;
	case STU_WEBSOCKET_CMD_RELIVE:
		break;

	case STU_WEBSOCKET_CMD_UPGRADE:
		break;
	case STU_WEBSOCKET_CMD_DEMOTE:
		break;

	case STU_WEBSOCKET_CMD_DISMISS:
		break;
	case STU_WEBSOCKET_CMD_ACTIVE:
		break;
	default:
		break;
	}

	stu_websocket_finalize_request(r, rc);
}

static stu_int_t
stu_websocket_process_request_frames(stu_websocket_request_t *r) {
	return STU_OK;
}

void
stu_websocket_finalize_request(stu_websocket_request_t *r, stu_int_t rc) {

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

