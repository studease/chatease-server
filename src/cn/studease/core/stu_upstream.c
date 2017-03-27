/*
 * stu_upstream.c
 *
 *  Created on: 2017-3-15
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_hash_t *stu_upstreams;

static stu_int_t stu_http_upstream_process_content_length(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_upstream_process_connection(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_upstream_process_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_upstream_process_unique_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);

stu_hash_t  stu_http_upstream_headers_in_hash;

stu_http_header_t  stu_http_upstream_headers_in[] = {
	{ stu_string("Server"), offsetof(stu_http_headers_out_t, server), stu_http_upstream_process_unique_header_line },

	{ stu_string("Content-Length"), offsetof(stu_http_headers_out_t, content_length), stu_http_upstream_process_content_length },
	{ stu_string("Content-Type"), offsetof(stu_http_headers_out_t, content_type), stu_http_upstream_process_header_line },
#if (STU_HTTP_GZIP)
	{ stu_string("Content-Encoding"), offsetof(stu_http_headers_out_t, content_encoding), stu_http_upstream_process_header_line },
#endif

	{ stu_string("Connection"), offsetof(stu_http_headers_out_t, connection), stu_http_upstream_process_connection },
	{ stu_string("Date"), offsetof(stu_http_headers_out_t, date), stu_http_upstream_process_header_line },

	{ stu_null_string, 0, NULL }
};

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

		c->upstream = u;
	} else {
		stu_upstream_cleanup(c);
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
	if (upstream->elts.size++ == s->weight) {
		q = (q == upstream->elts.queue.prev) ? stu_queue_head(&upstream->elts.queue) : stu_queue_next(q);
		e = stu_queue_data(q, stu_list_elt_t, queue);
		s = (stu_upstream_server_t *) e->obj;

		upstream->elts.size = 0;
	}

	u->server = s;
	u->peer.state = STU_UPSTREAM_PEER_BUSY;

	return STU_OK;
}

stu_int_t
stu_upstream_init(stu_connection_t *c) {
	stu_upstream_t *u;
	stu_int_t       rc;

	u = c->upstream;

	if (u->peer.state >= STU_UPSTREAM_PEER_CONNECTED) {
		u->peer.state = STU_UPSTREAM_PEER_CONNECTED;

		if (c->upstream->reinit_request(c) == STU_ERROR) {
			stu_log_error(0, "Failed to reinit request of upstream %s.", u->server->name.data);
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
	stu_upstream_t *u;
	stu_socket_t    fd;
	int             rc;
	stu_int_t       err;

	u = c->upstream;

	fd = socket(u->server->addr.sockaddr.sin_family, SOCK_STREAM, 0);
	if (fd == (stu_socket_t) STU_SOCKET_INVALID) {
		stu_log_error(stu_errno, "Failed to create socket for upstream %s, fd=%d.", u->server->name.data, c->fd);
		return STU_ERROR;
	}

	if (stu_nonblocking(fd) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while setting upstream %s, fd=%d.", u->server->name.data, c->fd);
		return STU_ERROR;
	}

	if (u->peer.connection == NULL) {
		u->peer.connection = stu_connection_get(fd);
		if (u->peer.connection == NULL) {
			stu_log_error(0, "Failed to get connection for upstream %s, fd=%d.", u->server->name.data, c->fd);
			return STU_ERROR;
		}

		u->peer.connection->data = u->create_request(c);
		if (u->peer.connection->data == NULL) {
			stu_log_error(0, "Failed to create request of upstream %s, fd=%d.", u->server->name.data, c->fd);
			return STU_ERROR;
		}
	}

	u->peer.connection->read.handler = u->read_event_handler;
	if (stu_epoll_add_event(&u->peer.connection->read, STU_READ_EVENT|EPOLLET) == STU_ERROR) {
		stu_log_error(0, "Failed to add read event of upstream %s, fd=%d.", u->server->name.data, c->fd);
		return STU_ERROR;
	}
	u->peer.connection->write.handler = u->write_event_handler;
	if (stu_epoll_add_event(&u->peer.connection->write, STU_WRITE_EVENT) == STU_ERROR) {
		stu_log_error(0, "Failed to add write event of upstream %s, fd=%d.", u->server->name.data, c->fd);
		return STU_ERROR;
	}
	u->peer.connection->read.data = u->peer.connection->write.data = c;

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

			stu_connection_close(u->peer.connection);
			u->peer.connection = NULL;

			return STU_DECLINED;
		}
	}

	u->peer.state = STU_UPSTREAM_PEER_CONNECTED;

	return STU_OK;
}

static stu_int_t
stu_upstream_next(stu_connection_t *c) {
	return STU_ERROR;
}


void *
stu_upstream_create_http_request(stu_connection_t *c) {
	stu_http_request_t *r;
	stu_upstream_t     *u;
	stu_connection_t   *pc;

	u = c->upstream;
	pc = u->peer.connection;

	r = stu_http_create_request(pc);
	if (r == NULL) {
		stu_log_error(0, "Failed to create http request of upstream %s, fd=%d.", u->server->name.data, c->fd);
		return NULL;
	}

	return r;
}

stu_int_t
stu_upstream_reinit_http_request(stu_connection_t *c) {
	stu_http_request_t *r;
	stu_upstream_t     *u;

	u = c->upstream;
	r = u->peer.connection->data;

	r->state = 0;

	return STU_OK;
}


void
stu_upstream_cleanup(stu_connection_t *c) {

}


static stu_int_t
stu_http_upstream_process_content_length(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_int_t  length;

	length = atol((const char *) h->value.data);
	r->headers_out.content_length_n = length;

	return stu_http_upstream_process_header_line(r, h, offset);
}

static stu_int_t
stu_http_upstream_process_connection(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	if (stu_strnstr(h->value.data, "Keep-Alive", h->value.len)) {
		r->headers_out.connection_type = STU_HTTP_CONNECTION_KEEP_ALIVE;
	} else {
		r->headers_out.connection_type = STU_HTTP_CONNECTION_CLOSE;
	}

	return STU_OK;
}

static stu_int_t
stu_http_upstream_process_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_table_elt_t  **ph;

	ph = (stu_table_elt_t **) ((char *) &r->headers_out + offset);
	if (*ph == NULL) {
		*ph = h;
	}

	return STU_OK;
}

static stu_int_t
stu_http_upstream_process_unique_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_table_elt_t  **ph;

	ph = (stu_table_elt_t **) ((char *) &r->headers_out + offset);
	if (*ph == NULL) {
		*ph = h;
		return STU_OK;
	}

	stu_log_error(0, "http upstream sent duplicate header line = > \"%s: %s\", "
			"previous value => \"%s: %s\"", h->key.data, h->value.data, &(*ph)->key.data, &(*ph)->value.data);

	return STU_HTTP_BAD_GATEWAY;
}
