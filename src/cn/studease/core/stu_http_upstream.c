/*
 * stu_http_upstream.c
 *
 *  Created on: 2017-6-19
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static stu_int_t stu_http_upstream_process_response_headers(stu_http_request_t *r);

static stu_int_t stu_http_upstream_process_content_length(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_upstream_process_connection(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_upstream_process_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_upstream_process_unique_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);

stu_hash_t         stu_http_upstream_headers_in_hash;
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

stu_conf_bitmask_t  stu_http_upstream_method_mask[] = {
	{ stu_string("GET"),  STU_HTTP_GET},
	{ stu_string("POST"), STU_HTTP_POST },
	{ stu_null_string, 0 }
};


void *
stu_http_upstream_create_request(stu_connection_t *c) {
	stu_upstream_t     *u;
	stu_connection_t   *pc;
	stu_http_request_t *pr;

	u = c->upstream;
	pc = u->peer.connection;

	pr = stu_http_create_request(pc);
	if (pr == NULL) {
		stu_log_error(0, "Failed to create http request for upstream %s, fd=%d.", u->server->name.data, c->fd);
		return NULL;
	}

	return pr;
}

stu_int_t
stu_http_upstream_reinit_request(stu_connection_t *c) {
	stu_upstream_t     *u;
	stu_connection_t   *pc;
	stu_http_request_t *pr;

	u = c->upstream;
	pc = u->peer.connection;
	pr = (stu_http_request_t *) pc->data;
	pr->state = 0;

	return STU_OK;
}

stu_int_t
stu_http_upstream_generate_request(stu_connection_t *c) {
	stu_upstream_t     *u;
	stu_connection_t   *pc;
	stu_http_request_t *r, *pr;
	stu_conf_bitmask_t *method;
	stu_str_t          *method_name;
	u_char             *p;
	size_t              size;

	r = (stu_http_request_t *) c->data;
	u = c->upstream;
	pc = u->peer.connection;
	pr = (stu_http_request_t *) pc->data;

	method_name = NULL;
	for (method = stu_http_upstream_method_mask; method->name.len; method++) {
		if (u->server->method == method->mask) {
			method_name = &method->name;
			break;
		}
	}

	if (method_name == NULL) {
		stu_log_error(0, "Http method unknown while generating upstream request: fd=%d, method=%hd.", c->fd, u->server->method);
		return STU_ERROR;
	}

	if (pc->buffer.start == NULL) {
		pc->buffer.start = (u_char *) stu_calloc(STU_HTTP_REQUEST_DEFAULT_SIZE);
		pc->buffer.end = pc->buffer.start + STU_HTTP_REQUEST_DEFAULT_SIZE;
	}

	p = stu_sprintf(pc->buffer.start, "%s %s%s HTTP/1.1" CRLF,
			method_name->data,
			u->server->target.data,
			u->server->method == STU_HTTP_GET ? (const char *) pr->request_body.start : "");
	p = stu_sprintf(p, "Host: ");
	p = stu_strncpy(p, r->headers_in.host->value.data, r->headers_in.host->value.len);
	p = stu_sprintf(p, CRLF);
	p = stu_sprintf(p, "User-Agent: " __NAME "/" __VERSION CRLF);
	p = stu_sprintf(p, "Accept: application/json" CRLF);
	p = stu_sprintf(p, "Accept-Charset: utf-8" CRLF);
	p = stu_sprintf(p, "Accept-Language: zh-CN,zh;q=0.8" CRLF);
	p = stu_sprintf(p, "Connection: keep-alive" CRLF);
	if (u->server->method == STU_HTTP_POST && pr->request_body.start && (size = pr->request_body.last - pr->request_body.start)) {
		p = stu_sprintf(p, "Content-Type: application/json" CRLF);
		p = stu_sprintf(p, "Content-Length: %ld" CRLF CRLF, size);
		p = stu_strncpy(p, pr->request_body.start, size);
	} else {
		p = stu_sprintf(p, CRLF);
	}
	*p = '\0';

	pc->buffer.last = p;

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

	stu_mutex_lock(&c->lock);

	if (pc == NULL || pc->fd == (stu_socket_t) STU_SOCKET_INVALID) {
		goto done;
	}

	if (pc->buffer.start == NULL) {
		pc->buffer.start = (u_char *) stu_calloc(STU_HTTP_REQUEST_DEFAULT_SIZE);
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

done:

	stu_mutex_unlock(&c->lock);
}

stu_int_t
stu_http_upstream_process_response(stu_connection_t *c) {
	stu_http_request_t *pr;
	stu_upstream_t     *u;
	stu_int_t           rc;

	u = c->upstream;
	pr = (stu_http_request_t *) u->peer.connection->data;

	rc = stu_http_upstream_process_response_headers(pr);
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
			h = stu_calloc(sizeof(stu_table_elt_t));
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

			h->lowcase_key = stu_calloc(h->key.len);
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

	stu_http_finalize_request(r, rc);
}

void
stu_http_upstream_write_handler(stu_event_t *ev) {
	stu_connection_t   *c, *pc;
	stu_upstream_t     *u;
	stu_int_t           n;

	c = (stu_connection_t *) ev->data;
	if (c->fd == (stu_socket_t) STU_SOCKET_INVALID || c->upstream == NULL) {
		return;
	}

	u = c->upstream;
	pc = u->peer.connection;

	// Lock pc rather than c
	stu_mutex_lock(&c->lock);

	if (pc == NULL || pc->fd == (stu_socket_t) STU_SOCKET_INVALID) {
		goto done;
	}

	if (u->peer.state >= STU_UPSTREAM_PEER_LOADING) {
		stu_log_debug(4, "upstream request already sent.");
		goto done;
	}

	n = send(pc->fd, pc->buffer.start, pc->buffer.last - pc->buffer.start, 0);
	if (n == -1) {
		stu_log_error(stu_errno, "Failed to send ident request, c->fd=%d, u->fd=%d.", c->fd, pc->fd);
		goto failed;
	}

	stu_log_debug(4, "sent to upstream %s: c->fd=%d, u->fd=%d, bytes=%d.", u->server->name.data, c->fd, pc->fd, n);

	u->peer.state = STU_UPSTREAM_PEER_LOADING;

	ev->data = pc;
	stu_event_del(&pc->write, STU_WRITE_EVENT, STU_READ_EVENT);
	ev->active = 0;

	goto done;

failed:

	u->cleanup_pt(c);

done:

	stu_mutex_unlock(&c->lock);
}


void stu_inline
stu_http_upstream_cleanup(stu_connection_t *c) {
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
			"previous value => \"%s: %s\"", h->key.data, h->value.data, (*ph)->key.data, (*ph)->value.data);

	return STU_HTTP_BAD_GATEWAY;
}
