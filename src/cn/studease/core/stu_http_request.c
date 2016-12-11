/*
 * stu_http_request.c
 *
 *  Created on: 2016-9-14
 *      Author: Tony Lau
 */

#include <sys/socket.h>
#include "stu_config.h"
#include "stu_core.h"

extern stu_cycle_t *stu_cycle;
extern stu_hash_t   stu_http_headers_in_hash;

static void stu_http_request_handler(stu_event_t *wev);
static stu_int_t stu_http_switch_protocol(stu_http_request_t *r);

static stu_int_t stu_http_process_request_headers(stu_http_request_t *r);

static stu_int_t stu_http_process_host(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_process_connection(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_process_sec_websocket_key(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);

static stu_int_t stu_http_process_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_process_unique_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);


static const stu_str_t  STU_HTTP_HEADER_SEC_WEBSOCKET_ACCEPT = stu_string("Sec-WebSocket-Accept");
static const stu_str_t  STU_HTTP_WEBSOCKET_SIGN_KEY = stu_string("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");


stu_http_header_t  stu_http_headers_in[] = {
	{ stu_string("Host"), offsetof(stu_http_headers_in_t, host), stu_http_process_host },
	{ stu_string("User-Agent"), offsetof(stu_http_headers_in_t, user_agent),  stu_http_process_header_line },

	{ stu_string("Accept"), offsetof(stu_http_headers_in_t, accept), stu_http_process_header_line },
	{ stu_string("Accept-Language"), offsetof(stu_http_headers_in_t, accept_language), stu_http_process_header_line },
	{ stu_string("Accept-Encoding"), offsetof(stu_http_headers_in_t, accept_encoding), stu_http_process_header_line },

	{ stu_string("Content-Length"), offsetof(stu_http_headers_in_t, content_length), stu_http_process_unique_header_line },
	{ stu_string("Content-Type"), offsetof(stu_http_headers_in_t, content_type), stu_http_process_header_line },

	{ stu_string("Sec-Websocket-Key"), offsetof(stu_http_headers_in_t, sec_websocket_key), stu_http_process_sec_websocket_key },
	{ stu_string("Sec-Websocket-Version"), offsetof(stu_http_headers_in_t, sec_websocket_version), stu_http_process_unique_header_line },
	{ stu_string("Sec-Websocket-Extensions"), offsetof(stu_http_headers_in_t, sec_websocket_extensions), stu_http_process_unique_header_line },
	{ stu_string("Upgrade"), offsetof(stu_http_headers_in_t, upgrade), stu_http_process_unique_header_line },

	{ stu_string("Connection"), offsetof(stu_http_headers_in_t, connection), stu_http_process_connection },

	{ stu_null_string, 0, NULL }
};


void
stu_http_wait_request_handler(stu_event_t *rev) {
	stu_connection_t *c;
	stu_int_t         n, err, retried;

	c = (stu_connection_t *) rev->data;

	stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) -1) {
		goto done;
	}

	if (c->buffer.start == NULL) {
		c->buffer.start = (u_char *) stu_base_palloc(c->pool, STU_HTTP_REQUEST_DEFAULT_SIZE);
		c->buffer.end = c->buffer.start + STU_HTTP_REQUEST_DEFAULT_SIZE;
	}
	c->buffer.last = c->buffer.start;
	stu_memzero(c->buffer.start, STU_HTTP_REQUEST_DEFAULT_SIZE);

	retried = 0;

again:

	n = recv(c->fd, c->buffer.start, STU_HTTP_REQUEST_DEFAULT_SIZE, 0);
	if (n == -1) {
		err = stu_errno;
		if (err == EAGAIN || err == EINTR) {
			if (retried++ >= 1) {
				stu_log_error(0, "recv aborted: fd=%d, err=%d.", c->fd, err);
				goto done;
			}

			stu_log_debug(0, "recv trying again: fd=%d, err=%d.", c->fd, err);
			goto again;
		}

		stu_log_error(err, "Failed to recv data: fd=%d.", c->fd);
		stu_connection_free(c);

		goto failed;
	}

	if (n == 0) {
		stu_log_debug(0, "Remote client has closed connection: fd=%d.", c->fd);
		goto failed;
	}

	stu_log_debug(0, "recv: fd=%d, bytes=%d.", c->fd, n);//str=\n%s, c->buffer.start

	c->data = (void *) stu_http_create_request(c);
	if (c->data == NULL) {
		stu_log_error(0, "Failed to create http request.");
		goto failed;
	}

	stu_http_process_request(c->data);

	goto done;

failed:

	c->read->active = FALSE;
	stu_epoll_del_event(c->read, STU_READ_EVENT);

	stu_http_close_connection(c);

done:

	stu_spin_unlock(&c->lock);
}

stu_http_request_t *
stu_http_create_request(stu_connection_t *c) {
	stu_http_request_t *r;

	if (c->data == NULL) {
		stu_spin_lock(&c->pool->lock);
		r = stu_base_pcalloc(c->pool, sizeof(stu_http_request_t));
		stu_spin_unlock(&c->pool->lock);
	} else {
		r = c->data;
	}

	r->connection = c;
	r->header_in = &c->buffer;
	stu_queue_init(&r->headers_in.headers.elts.queue);
	stu_queue_init(&r->headers_out.headers.elts.queue);

	return r;
}

void
stu_http_process_request(stu_http_request_t *r) {
	stu_int_t         rc;
	stu_connection_t *c;
	stu_str_t         channel_id;
	stu_uint_t        kh;
	u_char           *last;
	stu_channel_t    *ch;

	rc = stu_http_process_request_headers(r);
	if (rc != STU_OK) {
		if (rc < 0) {
			rc = STU_HTTP_BAD_REQUEST;
		}

		stu_log_error(0, "Failed to process request headers.");
		stu_http_finalize_request(r, rc);
		return;
	}

	c = r->connection;

	if (r->headers_in.upgrade) {
		last = r->uri.data + r->uri.len;

		channel_id.data = stu_strrchr(r->uri.data, last, '/');
		if (channel_id.data == NULL) {
			channel_id.data = r->uri.data;
		} else {
			channel_id.data++;
		}
		channel_id.len = last - channel_id.data;

		kh = stu_hash_key_lc(channel_id.data, channel_id.len);
		ch = stu_hash_find(&stu_cycle->channels, kh, channel_id.data, channel_id.len);
		if (ch == NULL) {
			ch = stu_slab_calloc(stu_cycle->slab_pool, sizeof(stu_channel_t) + channel_id.len + 1);
			if (ch == NULL) {
				stu_log_error(0, "Failed to alloc new channel.");
				goto failed;
			}

			if (stu_channel_init(ch, &channel_id) == STU_ERROR) {
				stu_log_error(0, "Failed to init channel.");
				goto failed;
			}

			if (stu_hash_init(&ch->userlist, NULL, STU_MAX_USER_N, stu_cycle->slab_pool,
					(stu_hash_palloc_pt) stu_slab_alloc, (stu_hash_free_pt) stu_slab_free) == STU_ERROR) {
				stu_log_error(0, "Failed to init userlist.");
				goto failed;
			}

			if (stu_hash_insert(&stu_cycle->channels, &channel_id, ch, STU_HASH_LOWCASE_KEY) == STU_ERROR) {
				stu_log_error(0, "Failed to insert channel.");
				goto failed;
			}

			stu_log_debug(0, "new channel(\"%s\"): kh=%lu, total=%lu.",
					ch->id.data, kh, stu_atomic_read(&stu_cycle->channels.length));
		}

		c->user.channel = ch;

		stu_log_debug(0, "inserting: %lu.", stu_atomic_read(&ch->userlist.lock.rlock.counter));
		if (stu_channel_insert(ch , c) == STU_ERROR) {
			goto failed;
		}

		rc = STU_HTTP_SWITCHING_PROTOCOLS;
	}

	stu_http_finalize_request(r, rc);

	return;

failed:

	stu_http_finalize_request(r, STU_HTTP_INTERNAL_SERVER_ERROR);
}


static stu_int_t
stu_http_process_request_headers(stu_http_request_t *r) {
	stu_int_t          rc;
	stu_uint_t         hk;
	stu_list_elt_t    *elt;
	stu_table_elt_t   *h;
	stu_http_header_t *hh;

	if (stu_http_parse_request_line(r, r->header_in) == STU_ERROR) {
		stu_log_error(0, "Failed to parse http request line.");
		return STU_ERROR;
	}

	for ( ;; ) {
		rc = stu_http_parse_header_line(r, r->header_in, 1);

		if (rc == STU_OK) {
			if (r->invalid_header) {
				stu_log_error(0, "client sent invalid header line: \"%s\"", r->header_name_start);
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

			elt = stu_base_pcalloc(r->connection->pool, sizeof(stu_list_elt_t));
			if (elt == NULL) {
				return STU_HTTP_INTERNAL_SERVER_ERROR;
			}
			elt->obj = h;
			elt->size = sizeof(stu_table_elt_t);
			stu_list_push(&r->headers_in.headers, elt);

			hk = stu_hash_key(h->lowcase_key, h->key.len);
			hh = stu_hash_find(&stu_http_headers_in_hash, hk, h->lowcase_key, h->key.len);
			if (hh) {
				rc = hh->handler(r, h, hh->offset);
				if (rc != STU_OK) {
					return rc;
				}
			}

			//stu_log_debug(0, "http header => \"%s: %s\"", h->key.data, h->value.data);

			continue;
		}

		if (rc == STU_DONE) {
			stu_log_debug(0, "the whole header has been parsed successfully.");
			return STU_OK;
		}

		if (rc == STU_AGAIN) {
			stu_log_debug(0, "a header line parsing is still not complete.");
			continue;
		}

		stu_log_error(0, "client sent invalid header line: \"%s\"", r->header_name_start);

		return STU_ERROR;
	}

	stu_log_error(0, "Unexpected error while processing request headers.");

	return STU_ERROR;
}


static stu_int_t
stu_http_process_host(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	return stu_http_process_unique_header_line(r, h, offset);
}

static stu_int_t
stu_http_process_connection(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	if (stu_strnstr(h->value.data, "Upgrade", h->value.len)) {
		r->headers_in.connection_type = STU_HTTP_CONNECTION_UPGRADE;
	} else {
		return STU_HTTP_NOT_IMPLEMENTED;
	}

	return STU_OK;
}

static stu_int_t
stu_http_process_sec_websocket_key(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_table_elt_t *e;
	stu_list_elt_t  *elt;
	stu_sha1_t       sha1;
	stu_str_t        sha1_signed;

	e = stu_base_pcalloc(r->connection->pool, sizeof(stu_table_elt_t));
	if (e == NULL) {
		return STU_HTTP_INTERNAL_SERVER_ERROR;
	}

	e->key.data = STU_HTTP_HEADER_SEC_WEBSOCKET_ACCEPT.data;
	e->key.len = STU_HTTP_HEADER_SEC_WEBSOCKET_ACCEPT.len;

	sha1_signed.len = SHA_DIGEST_LENGTH;
	sha1_signed.data = stu_base_pcalloc(r->connection->pool, sha1_signed.len + 1);

	e->value.len = stu_base64_encoded_length(SHA_DIGEST_LENGTH);
	e->value.data = stu_base_pcalloc(r->connection->pool, e->value.len + 1);

	if (sha1_signed.data == NULL || e->value.data == NULL) {
		return STU_HTTP_INTERNAL_SERVER_ERROR;
	}

	stu_sha1_init(&sha1);
	stu_sha1_update(&sha1, h->value.data, h->value.len);
	stu_sha1_update(&sha1, STU_HTTP_WEBSOCKET_SIGN_KEY.data, STU_HTTP_WEBSOCKET_SIGN_KEY.len);
	stu_sha1_final(sha1_signed.data, &sha1);

	stu_base64_encode(&e->value, &sha1_signed);

	r->headers_out.sec_websocket_accept = e;

	elt = stu_base_pcalloc(r->connection->pool, sizeof(stu_list_elt_t));
	if (elt == NULL) {
		return STU_HTTP_INTERNAL_SERVER_ERROR;
	}
	elt->obj = h;
	elt->size = sizeof(stu_table_elt_t);
	stu_list_push(&r->headers_out.headers, elt);

	return STU_OK;
}

static stu_int_t
stu_http_process_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_table_elt_t  **ph;

	ph = (stu_table_elt_t **) ((char *) &r->headers_in + offset);

	if (*ph == NULL) {
		*ph = h;
	}

	return STU_OK;
}

static stu_int_t
stu_http_process_unique_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_table_elt_t  **ph;

	ph = (stu_table_elt_t **) ((char *) &r->headers_in + offset);

	if (*ph == NULL) {
		*ph = h;
		return STU_OK;
	}

	stu_log_error(0, "client sent duplicate header line = > \"%s: %s\", "
			"previous value => \"%s: %s\"", h->key.data, h->value.data, &(*ph)->key.data, &(*ph)->value.data);

	return STU_HTTP_BAD_REQUEST;
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
	stu_buf_t          *buf;
	stu_int_t           n;

	c = (stu_connection_t *) wev->data;

	stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) -1) {
		goto done;
	}

	stu_epoll_del_event(c->write, STU_WRITE_EVENT);

	buf = &c->buffer;
	buf->last = buf->start;
	stu_memzero(buf->start, buf->end - buf->start);

	r = (stu_http_request_t *) c->data;
	if (r->headers_out.status == STU_HTTP_SWITCHING_PROTOCOLS) {
		buf->last = stu_memcpy(buf->last, "HTTP/1.1 101 Switching Protocols\r\n", 34);
		buf->last = stu_memcpy(buf->last, "Server: Chatease-Server/Beta\r\n", 30);
		buf->last = stu_memcpy(buf->last, "Upgrade: websocket\r\n", 20);
		buf->last = stu_memcpy(buf->last, "Connection: upgrade\r\n", 21);
		buf->last = stu_memcpy(buf->last, r->headers_out.sec_websocket_accept->key.data, r->headers_out.sec_websocket_accept->key.len);
		buf->last = stu_memcpy(buf->last, ": ", 2);
		buf->last = stu_memcpy(buf->last, r->headers_out.sec_websocket_accept->value.data, r->headers_out.sec_websocket_accept->value.len);
		buf->last = stu_memcpy(buf->last, "\r\n\r\n", 4);
	} else {
		buf->last = stu_memcpy(buf->last, "HTTP/1.1 400 Bad Request\r\nServer: Chatease-Server/Beta\r\nContent-type: text/html\r\nContent-length: 21\r\n\r\nChatease-Server/Beta\n", 124);
	}

	n = send(c->fd, buf->start, stu_strlen(buf->start), 0);
	if (n == -1) {
		stu_log_debug(0, "Failed to send data: fd=%d.", c->fd);
		goto failed;
	}

	stu_log_debug(0, "sent: fd=%d, bytes=%d.", c->fd, n);//str=\n%s, buf->start

	if (r->headers_out.status == STU_HTTP_SWITCHING_PROTOCOLS) {
		if (stu_http_switch_protocol(r) == STU_ERROR) {
			stu_log_error(0, "Failed to switch protocol: fd=%d.", c->fd);
			goto failed;
		}
	}

	goto done;

failed:

	c->read->active = FALSE;
	stu_epoll_del_event(c->read, STU_READ_EVENT);

	stu_http_close_connection(c);

done:

	stu_spin_unlock(&c->lock);
}

static stu_int_t
stu_http_switch_protocol(stu_http_request_t *r) {
	stu_connection_t *c;

	c = r->connection;

	stu_base_pool_reset(c->pool);

	c->buffer.start = c->buffer.last =c->buffer.end = NULL;
	c->data = NULL;

	c->read->handler = stu_websocket_wait_request_handler;
	c->write->handler = stu_websocket_request_handler;

	if (stu_epoll_add_event(c->read, STU_READ_EVENT|EPOLLET) == STU_ERROR) {
		stu_log_error(0, "Failed to add http client read event.");
		return STU_ERROR;
	}

	return STU_OK;
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
stu_http_empty_handler(stu_event_t *ev) {

}

void
stu_http_request_empty_handler(stu_http_request_t *r) {

}

