/*
 * stu_http_request.c
 *
 *  Created on: 2016-9-14
 *      Author: Tony Lau
 */

#include <sys/socket.h>
#include "stu_config.h"
#include "stu_core.h"

static void stu_http_request_handler(stu_event_t *wev);

static stu_int_t stu_http_process_request_headers(stu_http_request_t *r);


void
stu_http_wait_request_handler(stu_event_t *rev) {
	stu_connection_t *c;
	u_char            buf[2048];
	stu_int_t         n;

	c = (stu_connection_t *) rev->data;

	stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) -1) {
		goto done;
	}

	stu_memzero(buf, 2048);

	n = recv(c->fd, buf, 2048, 0);
	if (n == -1) {
		if (stu_errno == EAGAIN) {
			goto done;
		}

		stu_log_debug(0, "Failed to recv data: fd=%d.", c->fd);
		stu_connection_free(c);
		goto done;
	}

	if (n == 0) {
		stu_log_debug(0, "Remote client has closed connection: fd=%d.", c->fd);
		c->read->active = FALSE;
		stu_epoll_del_event(c->read, STU_READ_EVENT);
		stu_http_close_connection(c);
		goto done;
	}

	stu_log_debug(0, "recv: fd=%d, bytes=%d, str=\n%s", c->fd, n, buf);

	c->data = (void *) stu_http_create_request(c);
	if (c->data == NULL) {
		stu_log_error(0, "Failed to create http request.");
		goto done;
	}

	stu_http_process_request((stu_http_request_t *) c->data);

done:

	stu_spin_unlock(&c->lock);
}


stu_http_request_t *
stu_http_create_request(stu_connection_t *c) {
	stu_http_request_t *r;
	stu_int_t           size;

	size = sizeof(stu_http_request_t);

	stu_spin_lock(&c->pool->lock);

	if (c->pool->data.end - c->pool->data.last < size) {
		stu_log_error(0, "Failed to alloc http request.");
		return NULL;
	}

	r = (stu_http_request_t *) c->pool->data.last;
	c->pool->data.last += size;

	stu_spin_unlock(&c->pool->lock);

	r->connection = c;

	return r;
}

void
stu_http_process_request(stu_http_request_t *r) {
	if (stu_http_process_request_headers(r) == STU_ERROR) {
		stu_log_error(0, "Failed to process request header.");
		stu_http_finalize_request(r, STU_HTTP_BAD_REQUEST);
		return;
	}

	if (r->headers_in.upgrade == NULL) {
		stu_log_error(0, "Failed to process request header.");
		stu_http_finalize_request(r, STU_HTTP_NOT_IMPLEMENTED);
		return;
	}

	stu_http_finalize_request(r, STU_HTTP_OK);
}

static stu_int_t
stu_http_process_request_headers(stu_http_request_t *r) {
	r->headers_in.upgrade = (stu_table_elt_t *) -1;
	return STU_OK;
}


void
stu_http_finalize_request(stu_http_request_t *r, stu_int_t rc) {
	stu_connection_t *c;

	r->headers_out.status = rc;
	c = r->connection;

	c->write->handler = stu_http_request_handler;
	if (stu_epoll_add_event(c->write, STU_WRITE_EVENT|EPOLLET) == STU_ERROR) {
		stu_log_error(0, "Failed to add http client write event.");
		return;
	}
}

static void
stu_http_request_handler(stu_event_t *wev) {
	stu_http_request_t *r;
	stu_connection_t   *c;
	u_char             *buf;
	stu_int_t           n;

	c = (stu_connection_t *) wev->data;

	stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) -1) {
		goto done;
	}

	stu_epoll_del_event(c->write, STU_WRITE_EVENT);

	r = (stu_http_request_t *) c->data;

	buf = (u_char *) "HTTP/1.0 200 OK\r\nServer:Chatease/Beta\r\nContent-type:text/html\r\nContent-length:7\r\n\r\nHello!\n";

	n = send(c->fd, buf, stu_strlen(buf), 0);
	if (n == -1) {
		stu_log_debug(0, "Failed to send data: fd=%d.", c->fd);
		goto done;
	}

	stu_log_debug(0, "sent: fd=%d, bytes=%d, str=\n%s", c->fd, n, buf);

	if (r->headers_out.status == STU_HTTP_SWITCHING_PROTOCOLS) {
		c->read->handler = stu_websocket_wait_request_handler;
		c->write->handler = stu_websocket_request_handler;
	}

done:

	stu_spin_unlock(&c->lock);
}


void
stu_http_close_request(stu_http_request_t *r, stu_int_t rc) {
	stu_connection_t *c;

	c = r->connection;

	stu_http_free_request(r, rc);
	stu_http_close_connection(c);
}

void
stu_http_free_request(stu_http_request_t *r, stu_int_t rc) {
	r->connection->data = NULL;
}

void
stu_http_close_connection(stu_connection_t *c) {
	stu_connection_close(c);
}


void
stu_http_empty_handler(stu_event_t *wev) {

}

void
stu_http_request_empty_handler(stu_http_request_t *r) {

}

