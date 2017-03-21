/*
 * stu_upstream_ident.c
 *
 *  Created on: 2017-3-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_list_t        *stu_ident_upstream;

extern stu_hash_t  stu_http_headers_in_hash;

static const stu_str_t STU_UPSTREAM_IDENT_REQUEST = stu_string(
		"GET /websocket/data/userinfo.json?token=%s HTTP/1.1" CRLF
		"Host: 192.168.1.202" CRLF
		"Connection: keep-alive" CRLF
		"User-Agent: " __NAME CRLF
		"Accept: text/html" CRLF
		"Accept-Language: zh-CN,zh;q=0.8" CRLF CRLF
	);

static stu_int_t stu_upstream_ident_process_response_headers(stu_http_request_t *r);


void
stu_upstream_ident_read_handler(stu_event_t *ev) {
	stu_connection_t *c, *pc;
	stu_upstream_t   *u;
	stu_int_t         n, err;

	c = (stu_connection_t *) ev->data;
	u = c->upstream;
	pc = u->peer.connection;

	stu_spin_lock(&pc->lock);
	if (pc->fd == (stu_socket_t) STU_SOCKET_INVALID) {
		stu_log_error(0, "upstream %s waited a invalid fd=%d.", u->server->name.data, pc->fd);
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
		u->peer.state = STU_UPSTREAM_PEER_IDLE;
		goto failed;
	}

	if (n == 0) {
		stu_log_debug(4, "Remote upstream peer has closed connection: fd=%d.", pc->fd);
		u->peer.state = STU_UPSTREAM_PEER_IDLE;
		goto failed;
	}

	stu_log_debug(4, "upstream %s recv: fd=%d, bytes=%d.", u->server->name.data, pc->fd, n);//str=\n%s, c->buffer.start

	u->peer.state = STU_UPSTREAM_PEER_LOADED;
	u->process_response(c);

	u->peer.state = STU_UPSTREAM_PEER_COMPLETED;

	goto done;

failed:

	pc->read.active = FALSE;
	stu_epoll_del_event(&pc->read, STU_READ_EVENT);

	stu_http_close_connection(pc);

done:

	stu_spin_unlock(&pc->lock);
}

stu_int_t
stu_upstream_ident_process_response(stu_connection_t *c) {
	stu_http_request_t *r;
	stu_upstream_t     *u;
	stu_int_t           rc;

	u = c->upstream;
	r = (stu_http_request_t *) u->peer.connection->data;

	rc = stu_upstream_ident_process_response_headers(r);
	if (rc != STU_OK) {
		stu_log_error(0, "Failed to process upstream ident response headers.");
		u->finalize_handler(c, STU_HTTP_BAD_GATEWAY);
		return STU_ERROR;
	}

	if (u->analyze_response(c) == STU_ERROR) {
		stu_log_error(0, "Failed to analyze upstream ident response.");
		u->finalize_handler(c, STU_HTTP_INTERNAL_SERVER_ERROR);
		return STU_ERROR;
	}

	u->finalize_handler(c, STU_HTTP_SWITCHING_PROTOCOLS);

	return STU_OK;
}

static stu_int_t
stu_upstream_ident_process_response_headers(stu_http_request_t *r) {
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
				stu_log_error(0, "upstream replied invalid header line: \"%s\"", r->header_name_start);
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

			if (stu_list_push(&r->headers_in.headers, h, sizeof(stu_table_elt_t)) == STU_ERROR) {
				return STU_HTTP_INTERNAL_SERVER_ERROR;
			}

			hk = stu_hash_key(h->lowcase_key, h->key.len);
			hh = stu_hash_find_locked(&stu_http_headers_in_hash, hk, h->lowcase_key, h->key.len);
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
			stu_log_debug(4, "the whole header has been parsed successfully.");
			return STU_OK;
		}

		if (rc == STU_AGAIN) {
			stu_log_debug(4, "a header line parsing is still not complete.");
			continue;
		}

		stu_log_error(0, "upstream replied invalid header line: \"%s\"", r->header_name_start);

		return STU_ERROR;
	}

	stu_log_error(0, "Unexpected error while processing response headers.");

	return STU_ERROR;
}

void
stu_upstream_ident_finalize_handler(stu_connection_t *c, stu_int_t rc) {
	stu_http_request_t *r;

	r = (stu_http_request_t *) c->data;

	stu_http_finalize_request(r, rc);
}


void
stu_upstream_ident_write_handler(stu_event_t *ev) {
	stu_connection_t *c, *pc;
	stu_upstream_t   *u;
	u_char           *data, temp[STU_HTTP_REQUEST_DEFAULT_SIZE];
	stu_int_t         n;

	c = (stu_connection_t *) ev->data;
	u = c->upstream;
	pc = u->peer.connection;

	// Lock pc rather than c
	stu_spin_lock(&pc->lock);

	if (u->peer.state >= STU_UPSTREAM_PEER_LOADING) {
		stu_log_debug(5, "upstream request already sent.");
		goto done;
	}

	stu_memzero(temp, STU_HTTP_REQUEST_DEFAULT_SIZE);
	data = stu_sprintf(temp, (const char *) STU_UPSTREAM_IDENT_REQUEST.data, "ch1");

	n = send(c->upstream->peer.connection->fd, temp, data - temp, 0);
	if (n == -1) {
		stu_log_error(stu_errno, "Failed to send ident request, c->fd=%d, u->fd=%d.", c->fd, c->upstream->peer.connection->fd);
	}

	u->peer.state = STU_UPSTREAM_PEER_LOADING;

done:

	stu_spin_unlock(&pc->lock);
}
