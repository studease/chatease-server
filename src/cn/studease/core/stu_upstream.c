/*
 * stu_upstream.c
 *
 *  Created on: 2017-3-15
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_hash_t *stu_upstreams;

stu_str_t  STU_HTTP_UPSTREAM_IDENT = stu_string("ident");
stu_str_t  STU_HTTP_UPSTREAM_STATUS = stu_string("status");

static stu_int_t stu_upstream_connect(stu_connection_t *c);
static stu_int_t stu_upstream_next(stu_connection_t *c);


stu_int_t
stu_upstream_create(stu_connection_t *c, u_char *name, size_t len) {
	stu_uint_t             hk;
	stu_list_t            *upstream;
	stu_upstream_t        *u;
	stu_upstream_server_t *s;
	stu_queue_t           *q;
	stu_list_elt_t        *e;

	hk = stu_hash_key(name, len);
	upstream = stu_hash_find(stu_upstreams, hk, name, len);
	if (upstream == NULL || upstream->length == 0) {
		stu_log_error(0, "Upstream list is empty.");
		return STU_ERROR;
	}

	// get upstream
	u = c->upstream;
	if (u == NULL) {
		u = stu_base_pcalloc(c->pool, sizeof(stu_upstream_t));
		if (u == NULL) {
			stu_log_error(0, "Failed to pcalloc upstream for fd=%d.", c->fd);
			return STU_ERROR;
		}

		u->cleanup_pt = stu_upstream_cleanup;
		c->upstream = u;
	} else {
		u->cleanup_pt(c);
	}

	// get upstream server
	e = (stu_list_elt_t *) upstream->elts.obj;
	if (e == NULL) {
		q = stu_queue_head(&upstream->elts.queue);
		e = stu_queue_data(q, stu_list_elt_t, queue);

		upstream->elts.obj = e;  // current server
		upstream->elts.size = 0; // lost weight
	}

	s = (stu_upstream_server_t *) e->obj;
	while (upstream->elts.size++ >= s->weight) {
		q = stu_queue_next(&e->queue);
		if (q == stu_queue_sentinel(&upstream->elts.queue)) {
			q = stu_queue_head(&upstream->elts.queue);
		}

		e = stu_queue_data(q, stu_list_elt_t, queue);
		s = (stu_upstream_server_t *) e->obj;

		upstream->elts.obj = e;
		upstream->elts.size = 0;
	}

	u->server = s;

	return STU_OK;
}

stu_int_t
stu_upstream_init(stu_connection_t *c) {
	stu_upstream_t *u;
	stu_int_t       rc;

	u = c->upstream;

	if (u->peer.state) {
		if (c->upstream->reinit_request_pt(c) == STU_ERROR) {
			stu_log_error(0, "Failed to reinit request of upstream %s.", u->server->name.data);
			return STU_ERROR;
		}

		if (c->upstream->generate_request_pt(c) == STU_ERROR) {
			stu_log_error(0, "Failed to generate request of upstream %s, fd=%d.", u->server->name.data, c->fd);
			return STU_ERROR;
		}

		c->upstream->write_event_handler(&u->peer.connection->write);

		return STU_OK;
	}

	rc = stu_upstream_connect(c);
	if (rc == STU_ERROR) {
		stu_log_error(0, "Failed to connect upstream %s.", u->server->name.data);
		return STU_ERROR;
	}

	if (rc == STU_DECLINED) {
		return stu_upstream_next(c);
	}

	return STU_OK;
}

static stu_int_t
stu_upstream_connect(stu_connection_t *c) {
	stu_upstream_t   *u;
	stu_connection_t *pc;
	stu_socket_t      fd;
	int               rc;
	stu_int_t         err;

	u = c->upstream;
	pc = u->peer.connection;

	fd = socket(u->server->addr.sockaddr.sin_family, SOCK_STREAM, 0);
	if (fd == (stu_socket_t) STU_SOCKET_INVALID) {
		stu_log_error(stu_errno, "Failed to create socket for upstream %s, fd=%d.", u->server->name.data, c->fd);
		return STU_ERROR;
	}

	if (stu_nonblocking(fd) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while setting upstream %s, fd=%d.", u->server->name.data, c->fd);
		return STU_ERROR;
	}

	if (pc == NULL) {
		pc = stu_connection_get(fd);
		if (pc == NULL) {
			stu_log_error(0, "Failed to get connection for upstream %s, fd=%d.", u->server->name.data, c->fd);
			return STU_ERROR;
		}

		u->peer.connection = pc;

		pc->data = u->create_request_pt(c);
		if (pc->data == NULL) {
			stu_log_error(0, "Failed to create request of upstream %s, fd=%d.", u->server->name.data, c->fd);
			return STU_ERROR;
		}
	}

	if (c->upstream->generate_request_pt(c) == STU_ERROR) {
		stu_log_error(0, "Failed to generate request of upstream %s, fd=%d.", u->server->name.data, c->fd);
		return STU_ERROR;
	}

	pc->read.handler = u->read_event_handler;
	if (stu_event_add(&pc->read, STU_READ_EVENT, STU_CLEAR_EVENT) == STU_ERROR) {
		stu_log_error(0, "Failed to add read event of upstream %s, fd=%d.", u->server->name.data, c->fd);
		return STU_ERROR;
	}
	pc->write.handler = u->write_event_handler;
	if (stu_event_add(&pc->write, STU_WRITE_EVENT, STU_CLEAR_EVENT) == STU_ERROR) {
		stu_log_error(0, "Failed to add write event of upstream %s, fd=%d.", u->server->name.data, c->fd);
		return STU_ERROR;
	}
	pc->read.data = pc->write.data = c;

	rc = connect(fd, (struct sockaddr *) &u->server->addr.sockaddr, u->server->addr.socklen);
	if (rc == -1) {
		err = stu_errno;
		if (err != EINPROGRESS
#if (STU_WIN32)
				/* Winsock returns WSAEWOULDBLOCK (NGX_EAGAIN) */
				&& err != EAGAIN
#endif
				) {
			if (err == ECONNREFUSED
#if (STU_LINUX)
					/*
					 * Linux returns EAGAIN instead of ECONNREFUSED
					 * for unix sockets if listen queue is full
					 */
					|| err == EAGAIN
#endif
					|| err == ECONNRESET
					|| err == ENETDOWN
					|| err == ENETUNREACH
					|| err == EHOSTDOWN
					|| err == EHOSTUNREACH) {

			} else {

			}
			stu_log_error(err, "Failed to connect to upstream %s, fd=%d.", u->server->name.data, c->fd);

			return STU_DECLINED;
		}
	}

	u->peer.state = STU_UPSTREAM_PEER_CONNECTED;

	return STU_OK;
}

static stu_int_t
stu_upstream_next(stu_connection_t *c) {
	stu_uint_t             hk;
	stu_list_t            *upstream;
	stu_upstream_t        *u;
	stu_upstream_server_t *s;
	stu_queue_t           *q;
	stu_list_elt_t        *e;

	u = c->upstream;

	stu_atomic_fetch_add(&u->server->fails, 1);

	hk = stu_hash_key(u->server->name.data, u->server->name.len);
	upstream = stu_hash_find(stu_upstreams, hk, u->server->name.data, u->server->name.len);
	if (upstream == NULL || upstream->length == 0) {
		stu_log_error(0, "Upstream list is empty.");
		return STU_ERROR;
	}

	// get upstream server
	e = (stu_list_elt_t *) upstream->elts.obj;
	if (e == NULL) {
		q = stu_queue_head(&upstream->elts.queue);
		e = stu_queue_data(q, stu_list_elt_t, queue);

		upstream->elts.obj = e;
		upstream->elts.size = 0;
	}

	s = (stu_upstream_server_t *) e->obj;
	while (upstream->elts.size++ >= s->weight) {
		q = stu_queue_next(&e->queue);
		if (q == stu_queue_sentinel(&upstream->elts.queue)) {
			q = stu_queue_head(&upstream->elts.queue);
		}

		e = stu_queue_data(q, stu_list_elt_t, queue);
		s = (stu_upstream_server_t *) e->obj;

		upstream->elts.obj = e;
		upstream->elts.size = 0;
	}

	u->server = s;

	return stu_upstream_init(c);
}

void
stu_upstream_cleanup(stu_connection_t *c) {
	stu_upstream_t   *u;
	stu_connection_t *pc;

	if (c->upstream == NULL) {
		return;
	}

	u = c->upstream;
	pc = u->peer.connection;

	if (pc) {
		stu_log_debug(4, "cleaning up upstream: fd=%d.", pc->fd);
/*
		pc->read.active = 1;
		pc->read.data = pc;
		stu_event_del(&pc->read, STU_READ_EVENT, 0);
*/
		pc->write.active = 0;
		pc->write.data = pc;
		stu_event_del(&pc->write, STU_WRITE_EVENT, 0);

		stu_connection_close(pc);
	}

	u->peer.connection = NULL;
	u->peer.state = STU_UPSTREAM_PEER_IDLE;
}
