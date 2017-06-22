/*
 * stu_http_upstream.c
 *
 *  Created on: 2017-6-19
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

extern stu_cycle_t *stu_cycle;
stu_hash_t          stu_http_upstream_headers_in_hash;

static stu_int_t stu_http_upstream_process_response_headers(stu_http_request_t *r);

static stu_int_t stu_http_upstream_process_content_length(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_upstream_process_connection(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_upstream_process_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_upstream_process_unique_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);

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


void *
stu_http_upstream_create_request(stu_connection_t *c) {
	stu_http_request_t *r;
	stu_upstream_t     *u;
	stu_connection_t   *pc;

	u = c->upstream;
	pc = u->peer.connection;

	r = stu_http_create_request(pc);
	if (r == NULL) {
		stu_log_error(0, "Failed to create http request for upstream %s, fd=%d.", u->server->name.data, c->fd);
		return NULL;
	}

	return r;
}

stu_int_t
stu_http_upstream_reinit_request(stu_connection_t *c) {
	stu_http_request_t *r;
	stu_upstream_t     *u;

	u = c->upstream;
	r = u->peer.connection->data;

	r->state = 0;

	return STU_OK;
}

stu_int_t
stu_http_upstream_generate_request(stu_connection_t *c) {
	return STU_OK;
}


void
stu_http_upstream_read_handler(stu_event_t *ev) {
	stu_connection_t *c, *pc;
	stu_upstream_t   *u;
	stu_int_t         n, err;

	c = (stu_connection_t *) ev->data;
	u = c->upstream;
	pc = u->peer.connection;

	stu_spin_lock(&c->lock);

	if (pc == NULL || pc->fd == (stu_socket_t) STU_SOCKET_INVALID) {
		goto done;
	}

	if (pc->buffer.start == NULL) {
		pc->buffer.start = (u_char *) stu_base_palloc(pc->pool, STU_HTTP_REQUEST_DEFAULT_SIZE);
		pc->buffer.end = pc->buffer.start + STU_HTTP_REQUEST_DEFAULT_SIZE;
	}
	pc->buffer.last = pc->buffer.start;
	stu_memzero(pc->buffer.start, STU_HTTP_REQUEST_DEFAULT_SIZE);

again:

	n = recv(pc->fd, pc->buffer.start, STU_HTTP_REQUEST_DEFAULT_SIZE, 0);
	if (n == -1) {
		err = stu_errno;
		if (err == EAGAIN) {
			stu_log_debug(4, "Already received by other threads: fd=%d, errno=%d.", pc->fd, err);
			goto done;
		}

		if (err == EINTR) {
			stu_log_debug(4, "recv trying again: fd=%d, errno=%d.", pc->fd, err);
			goto again;
		}

		stu_log_error(err, "Failed to recv data: fd=%d.", pc->fd);
		goto failed;
	}

	if (n == 0) {
		stu_log_debug(4, "Remote upstream peer has closed connection: fd=%d.", pc->fd);
		goto failed;
	}

	stu_log_debug(4, "upstream %s recv: fd=%d, bytes=%d.", u->server->name.data, pc->fd, n);//str=\n%s, c->buffer.start

	u->peer.state = STU_UPSTREAM_PEER_LOADED;
	u->process_response_pt(c);

	u->peer.state = STU_UPSTREAM_PEER_COMPLETED;

	goto done;

failed:

	u->cleanup_pt(c);

	u->peer.connection = NULL;
	u->peer.state = STU_UPSTREAM_PEER_IDLE;

done:

	stu_spin_unlock(&c->lock);
}

stu_int_t
stu_http_upstream_process_response(stu_connection_t *c) {
	stu_http_request_t *r;
	stu_upstream_t     *u;
	stu_int_t           rc;

	u = c->upstream;
	r = (stu_http_request_t *) u->peer.connection->data;

	rc = stu_http_upstream_process_response_headers(r);
	if (rc != STU_OK) {
		stu_log_error(0, "Failed to process upstream ident response headers.");
		u->finalize_handler_pt(c, STU_HTTP_BAD_GATEWAY);
		return STU_ERROR;
	}

	if (u->analyze_response_pt(c) == STU_ERROR) {
		//stu_log_error(0, "Failed to analyze upstream ident response.");
		u->finalize_handler_pt(c, STU_HTTP_INTERNAL_SERVER_ERROR);
		return STU_ERROR;
	}

	return STU_OK;
}

static stu_int_t
stu_http_upstream_process_response_headers(stu_http_request_t *r) {
	stu_int_t          rc;
	stu_uint_t         hk;
	stu_table_elt_t   *h;
	stu_http_header_t *hh;

	if (stu_http_parse_status_line(r, r->header_in) == STU_ERROR) {
		stu_log_error(0, "Failed to parse http status line.");
		return STU_ERROR;
	}

	for ( ;; ) {
		rc = stu_http_parse_header_line(r, r->header_in, 1);

		if (rc == STU_OK) {
			if (r->invalid_header) {
				stu_log_error(0, "Upstream replied invalid header line: \"%s\"", r->header_name_start);
				continue;
			}

			/* a header line has been parsed successfully */
			h = stu_base_pcalloc(r->connection->pool, sizeof(stu_table_elt_t));
			if (h == NULL) {
				return STU_HTTP_INTERNAL_SERVER_ERROR;
			}

			h->hash = r->header_hash;

			h->key.len = r->header_name_end - r->header_name_start;
			h->key.data = r->header_name_start;
			h->key.data[h->key.len] = '\0';

			h->value.len = r->header_end - r->header_start;
			h->value.data = r->header_start;
			h->value.data[h->value.len] = '\0';

			h->lowcase_key = stu_base_pcalloc(r->connection->pool, h->key.len);
			if (h->lowcase_key == NULL) {
				return STU_HTTP_INTERNAL_SERVER_ERROR;
			}

			if (h->key.len == r->lowcase_index) {
				memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
			} else {
				stu_strlow(h->lowcase_key, h->key.data, h->key.len);
			}

			if (stu_list_push(&r->headers_out.headers, h, sizeof(stu_table_elt_t)) == STU_ERROR) {
				return STU_HTTP_INTERNAL_SERVER_ERROR;
			}

			hk = stu_hash_key(h->lowcase_key, h->key.len);
			hh = stu_hash_find_locked(&stu_http_upstream_headers_in_hash, hk, h->lowcase_key, h->key.len);
			if (hh) {
				rc = hh->handler(r, h, hh->offset);
				if (rc != STU_OK) {
					return rc;
				}
			}

			//stu_log_debug(4, "http header => \"%s: %s\"", h->key.data, h->value.data);

			continue;
		}

		if (rc == STU_DONE) {
			stu_log_debug(4, "The whole header has been parsed successfully.");
			return STU_OK;
		}

		if (rc == STU_AGAIN) {
			stu_log_debug(4, "A header line parsing is still not complete.");
			continue;
		}

		stu_log_error(0, "Upstream replied invalid header line: \"%s\"", r->header_name_start);

		return STU_ERROR;
	}

	stu_log_error(0, "Unexpected error while processing response headers.");

	return STU_ERROR;
}

stu_int_t
stu_http_upstream_analyze_response(stu_connection_t *c) {
	stu_upstream_t     *u;
	stu_connection_t   *pc;
	stu_http_request_t *pr;

	u = c->upstream;
	pc = u->peer.connection;
	pr = (stu_http_request_t *) pc->data;

	u->finalize_handler_pt(c, pr->headers_out.status);

	return STU_OK;
}

void
stu_http_upstream_finalize_handler(stu_connection_t *c, stu_int_t rc) {
	stu_http_request_t *r;
	stu_upstream_t     *u;

	r = (stu_http_request_t *) c->data;
	u = c->upstream;

	u->cleanup_pt(c);

	u->peer.connection = NULL;
	u->peer.state = STU_UPSTREAM_PEER_IDLE;

	stu_http_finalize_request(r, rc);
}

void
stu_http_upstream_write_handler(stu_event_t *ev) {
	stu_connection_t   *c, *pc;
	stu_http_request_t *pr;
	stu_upstream_t     *u;
	stu_int_t           n;

	c = (stu_connection_t *) ev->data;
	if (c->fd == (stu_socket_t) STU_SOCKET_INVALID || c->upstream == NULL) {
		return;
	}

	u = c->upstream;
	pc = u->peer.connection;
	pr = (stu_http_request_t *) pc->data;

	// Lock pc rather than c
	stu_spin_lock(&c->lock);

	if (pc == NULL || pc->fd == (stu_socket_t) STU_SOCKET_INVALID) {
		goto done;
	}

	if (u->peer.state >= STU_UPSTREAM_PEER_LOADING) {
		stu_log_debug(4, "upstream request already sent.");
		goto done;
	}

	n = send(pc->fd, pr->request_body.start, pr->request_body.last - pr->request_body.start, 0);
	if (n == -1) {
		stu_log_error(stu_errno, "Failed to send ident request, c->fd=%d, u->fd=%d.", c->fd, pc->fd);
		goto failed;
	}

	stu_log_debug(4, "sent to upstream %s: fd=%d, bytes=%d.", u->server->name.data, pc->fd, n);

	u->peer.state = STU_UPSTREAM_PEER_LOADING;

	ev->data = pc;
	stu_event_del(&pc->write, STU_WRITE_EVENT, 0);
	ev->active = 0;

	goto done;

failed:

	u->cleanup_pt(c);

	u->peer.connection = NULL;
	u->peer.state = STU_UPSTREAM_PEER_IDLE;

done:

	stu_spin_unlock(&c->lock);
}


void
stu_http_upstream_cleanup(stu_connection_t *c) {
	stu_http_request_t *pr;

	pr = (stu_http_request_t *) c->upstream->peer.connection->data;
	if (pr->request_body.start) {
		stu_slab_free(stu_cycle->slab_pool, pr->request_body.start);
		pr->request_body.start = NULL;
	}

	stu_upstream_cleanup(c);
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
