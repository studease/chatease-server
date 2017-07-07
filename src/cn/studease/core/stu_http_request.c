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
static stu_int_t stu_http_switch_protocol(stu_http_request_t *r);

static stu_int_t stu_http_process_request_headers(stu_http_request_t *r);

static stu_int_t stu_http_process_host(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_process_connection(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_process_content_length(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_process_sec_websocket_key(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_process_sec_websocket_key_for_safari(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_process_sec_websocket_protocol(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);

static stu_int_t stu_http_process_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);
static stu_int_t stu_http_process_unique_header_line(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);


static const stu_str_t  STU_HTTP_HEADER_SEC_WEBSOCKET_ACCEPT = stu_string("Sec-WebSocket-Accept");
static const stu_str_t  STU_HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL = stu_string("Sec-WebSocket-Protocol");
static const stu_str_t  STU_HTTP_WEBSOCKET_SIGN_KEY = stu_string("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");


extern stu_cycle_t *stu_cycle;
extern stu_int_t    stu_preview_auto_id;

extern stu_str_t  STU_HTTP_UPSTREAM_IDENT;
extern stu_str_t  STU_HTTP_UPSTREAM_IDENT_RESPONSE;

extern stu_str_t  STU_FLASH_POLICY_REQUEST;
extern stu_str_t  STU_FLASH_POLICY_FILE;

stu_hash_t         stu_http_headers_in_hash;
stu_http_header_t  stu_http_headers_in[] = {
	{ stu_string("Host"), offsetof(stu_http_headers_in_t, host), stu_http_process_host },
	{ stu_string("User-Agent"), offsetof(stu_http_headers_in_t, user_agent),  stu_http_process_header_line },

	{ stu_string("Accept"), offsetof(stu_http_headers_in_t, accept), stu_http_process_header_line },
	{ stu_string("Accept-Language"), offsetof(stu_http_headers_in_t, accept_language), stu_http_process_header_line },
#if (STU_HTTP_GZIP)
	{ stu_string("Accept-Encoding"), offsetof(stu_http_headers_in_t, accept_encoding), stu_http_process_header_line },
#endif

	{ stu_string("Content-Length"), offsetof(stu_http_headers_in_t, content_length), stu_http_process_content_length },
	{ stu_string("Content-Type"), offsetof(stu_http_headers_in_t, content_type), stu_http_process_header_line },

	{ stu_string("Sec-Websocket-Key"), offsetof(stu_http_headers_in_t, sec_websocket_key), stu_http_process_sec_websocket_key },
	{ stu_string("Sec-Websocket-Key1"), offsetof(stu_http_headers_in_t, sec_websocket_key1), stu_http_process_sec_websocket_key_for_safari },
	{ stu_string("Sec-Websocket-Key2"), offsetof(stu_http_headers_in_t, sec_websocket_key2), stu_http_process_sec_websocket_key_for_safari },
	{ stu_string("(Key3)"), offsetof(stu_http_headers_in_t, sec_websocket_key3), stu_http_process_sec_websocket_key_for_safari },
	{ stu_string("Sec-Websocket-Protocol"), offsetof(stu_http_headers_in_t, sec_websocket_protocol), stu_http_process_sec_websocket_protocol },
	{ stu_string("Sec-Websocket-Version"), offsetof(stu_http_headers_in_t, sec_websocket_version), stu_http_process_unique_header_line },
	{ stu_string("Sec-Websocket-Extensions"), offsetof(stu_http_headers_in_t, sec_websocket_extensions), stu_http_process_unique_header_line },
	{ stu_string("Upgrade"), offsetof(stu_http_headers_in_t, upgrade), stu_http_process_header_line },

	{ stu_string("Connection"), offsetof(stu_http_headers_in_t, connection), stu_http_process_connection },

	{ stu_null_string, 0, NULL }
};


void
stu_http_wait_request_handler(stu_event_t *rev) {
	stu_connection_t *c;
	stu_int_t         n, err;

	c = (stu_connection_t *) rev->data;

	stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) STU_SOCKET_INVALID) {
		goto done;
	}

	if (c->buffer.start == NULL) {
		c->buffer.start = (u_char *) stu_base_palloc(c->pool, STU_HTTP_REQUEST_DEFAULT_SIZE);
		c->buffer.end = c->buffer.start + STU_HTTP_REQUEST_DEFAULT_SIZE;
	}
	c->buffer.last = c->buffer.start;
	stu_memzero(c->buffer.start, STU_HTTP_REQUEST_DEFAULT_SIZE);

again:

	n = recv(c->fd, c->buffer.start, STU_HTTP_REQUEST_DEFAULT_SIZE, 0);
	if (n == -1) {
		err = stu_errno;
		if (err == EAGAIN) {
			stu_log_debug(4, "Already received by other threads: fd=%d, errno=%d.", c->fd, err);
			goto done;
		}

		if (err == EINTR) {
			stu_log_debug(4, "recv trying again: fd=%d, errno=%d.", c->fd, err);
			goto again;
		}

		stu_log_error(err, "Failed to recv data: fd=%d.", c->fd);
		goto failed;
	}

	if (n == 0) {
		stu_log_debug(4, "Remote client has closed connection: fd=%d.", c->fd);
		goto failed;
	}

	stu_log_debug(4, "recv: fd=%d, bytes=%d.", c->fd, n); //str=\n%s, c->buffer.start

	if (stu_strncmp(c->buffer.start, STU_FLASH_POLICY_REQUEST.data, STU_FLASH_POLICY_REQUEST.len) == 0) {
		n = send(c->fd, STU_FLASH_POLICY_FILE.data, STU_FLASH_POLICY_FILE.len, 0);
		if (n == -1) {
			stu_log_debug(4, "Failed to send policy file: fd=%d.", c->fd);
			goto failed;
		}

		stu_log_debug(4, "sent policy file: fd=%d, bytes=%d.", c->fd, n);

		goto done;
	}

	c->data = (void *) stu_http_create_request(c);
	if (c->data == NULL) {
		stu_log_error(0, "Failed to create http request.");
		goto failed;
	}

	stu_http_process_request(c->data);

	goto done;

failed:

	c->read.active = 0;
	stu_event_del(&c->read, STU_READ_EVENT, 0);

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

	if (r == NULL) {
		stu_log_error(0, "Failed to create http request.");
		return NULL;
	}

	r->connection = c;
	r->header_in = &c->buffer;
	stu_list_init(&r->headers_in.headers, c->pool, (stu_list_palloc_pt) stu_base_pcalloc, NULL);
	stu_list_init(&r->headers_out.headers, c->pool, (stu_list_palloc_pt) stu_base_pcalloc, NULL);

	return r;
}

void
stu_http_process_request(stu_http_request_t *r) {
	stu_int_t           rc;
	stu_connection_t   *c;
	stu_table_elt_t    *protocol;
	stu_int_t           m, n, size, extened;
	stu_str_t           cid, name, role, state;
	u_char             *d, *s, *p, opcode, temp[STU_HTTP_REQUEST_DEFAULT_SIZE], buf[STU_USER_ID_MAX_LEN];
	stu_channel_t      *ch;

	rc = stu_http_process_request_headers(r);
	if (rc != STU_OK) {
		stu_log_error(0, "Failed to process request headers.");
		stu_http_finalize_request(r, STU_HTTP_BAD_REQUEST);
		return;
	}

	c = r->connection;

	if (r->headers_in.upgrade == NULL) {
		stu_http_finalize_request(r, STU_HTTP_NOT_IMPLEMENTED);
		return;
	}

	if (stu_cycle->config.edition == PREVIEW) {
		goto preview;
	}

	// enterprise
	if (stu_upstream_create(c, STU_HTTP_UPSTREAM_IDENT.data, STU_HTTP_UPSTREAM_IDENT.len) == STU_ERROR) {
		stu_log_error(0, "Failed to create http upstream \"ident\".");
		goto failed;
	}

	c->upstream->read_event_handler = stu_http_upstream_read_handler;
	c->upstream->write_event_handler = stu_http_upstream_write_handler;

	c->upstream->create_request_pt = stu_http_upstream_create_request;
	c->upstream->reinit_request_pt = stu_http_upstream_reinit_request;
	c->upstream->generate_request_pt = stu_http_upstream_ident_generate_request;
	c->upstream->process_response_pt = stu_http_upstream_process_response;
	c->upstream->analyze_response_pt = stu_http_upstream_ident_analyze_response;
	c->upstream->finalize_handler_pt = stu_http_upstream_finalize_handler;
	c->upstream->cleanup_pt = stu_http_upstream_cleanup;

	if (stu_upstream_init(c) == STU_ERROR) {
		stu_log_error(0, "Failed to init upstream.");
		goto failed;
	}

	return;

preview:

	// get channel ID
	cid.data = stu_base_pcalloc(c->pool, r->target.len + 1);
	if (cid.data == NULL) {
		stu_log_error(0, "Failed to pcalloc memory for channel id, fd=%d.", c->fd);
		goto failed;
	}
	stu_strncpy(cid.data, r->target.data, r->target.len);
	cid.len = r->target.len;

	// reset user info
	s = stu_sprintf(buf, "%ld", stu_preview_auto_id++);
	*s = '\0';

	c->user.id.len = s - buf;
	c->user.id.data = stu_base_pcalloc(c->pool, c->user.id.len + 1);
	if (c->user.id.data == NULL) {
		stu_log_error(0, "Failed to pcalloc memory for user id, fd=%d.", c->fd);
		goto failed;
	}
	stu_strncpy(c->user.id.data, buf, c->user.id.len);

	if (stu_http_arg(r, STU_PROTOCOL_NAME.data, STU_PROTOCOL_NAME.len, &name) != STU_OK) {
		stu_log_error(0, "User name not specified, fd=%d.", c->fd);
		goto failed;
	}

	d = s = name.data;
	stu_unescape_uri(&d, &s, name.len, 0);
	name.len = d - name.data;

	c->user.name.data = stu_base_pcalloc(c->pool, name.len + 1);
	if (c->user.name.data == NULL) {
		stu_log_error(0, "Failed to pcalloc memory for user name, fd=%d.", c->fd);
		goto failed;
	}
	stu_strncpy(c->user.name.data, name.data, name.len);
	c->user.name.len = name.len;

	c->user.role = STU_USER_ROLE_NORMAL;
	if (stu_http_arg(r, STU_PROTOCOL_ROLE.data, STU_PROTOCOL_ROLE.len, &role) == STU_OK) {
		m = atoi((const char *) role.data);
		stu_user_set_role(&c->user, m & 0xFF);
	}

	// insert user into channel
	if (stu_channel_insert(&cid, c) == STU_ERROR) {
		stu_log_error(0, "Failed to insert connection: fd=%d.", c->fd);
		goto failed;
	}

	ch = c->user.channel;

	if (stu_http_arg(r, STU_PROTOCOL_STATE.data, STU_PROTOCOL_STATE.len, &state) == STU_OK) {
		if (c->user.role & 0xF0) {
			m = atoi((const char *) state.data);
			ch->state = m & 0xFF;
		}
	}

	// finalize request
	protocol = r->headers_out.sec_websocket_protocol;
	if (protocol && stu_strncmp("binary", protocol->value.data, protocol->value.len) == 0) {
		opcode = STU_WEBSOCKET_OPCODE_BINARY;
	} else {
		opcode = STU_WEBSOCKET_OPCODE_TEXT;
	}

	stu_http_finalize_request(r, STU_HTTP_SWITCHING_PROTOCOLS);

	p = stu_sprintf(
			(u_char *) temp + 10, (const char *) STU_HTTP_UPSTREAM_IDENT_RESPONSE.data,
			c->user.id.data, c->user.name.data, c->user.role,
			ch->id.data, ch->state, ch->userlist.length
		);
	*p = '\0';

	size = p - temp - 10;
	p = stu_websocket_encode_frame(opcode, temp, size, &extened);

	n = send(c->fd, p, size + 2 + extened, 0);
	if (n == -1) {
		stu_log_debug(4, "Failed to send \"ident\" frame: fd=%d.", c->fd);
		goto failed;
	}

	stu_log_debug(4, "sent: fd=%d, bytes=%d.", c->fd, n);

	return;

failed:

	stu_http_finalize_request(r, STU_HTTP_INTERNAL_SERVER_ERROR);
}


static stu_int_t
stu_http_process_request_headers(stu_http_request_t *r) {
	stu_int_t          rc;
	stu_uint_t         hk;
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

			h->lowcase_key = stu_base_pcalloc(r->connection->pool, h->key.len + 1);
			if (h->lowcase_key == NULL) {
				return STU_HTTP_INTERNAL_SERVER_ERROR;
			}

			if (h->key.len == r->lowcase_index) {
				memcpy(h->lowcase_key, r->lowcase_header, h->key.len);
			} else {
				stu_strlow(h->lowcase_key, h->key.data, h->key.len);
			}
			h->lowcase_key[h->key.len] = '\0';

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
			stu_log_debug(4, "The whole header has been parsed successfully.");
			return STU_OK;
		}

		if (rc == STU_AGAIN) {
			stu_log_debug(4, "A header line parsing is still not complete.");
			continue;
		}

		stu_log_error(0, "Unexpected error while processing request headers.");
		break;
	}

	return STU_ERROR;
}


static stu_int_t
stu_http_process_host(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	return stu_http_process_unique_header_line(r, h, offset);
}

static stu_int_t
stu_http_process_connection(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_http_process_unique_header_line(r, h, offset);

	if (stu_strnstr(h->value.data, "Keep-Alive", h->value.len)) {
		r->headers_in.connection_type = STU_HTTP_CONNECTION_KEEP_ALIVE;
	} else if (stu_strnstr(h->value.data, "Upgrade", h->value.len)) {
		r->headers_in.connection_type = STU_HTTP_CONNECTION_UPGRADE;
	} else {
		r->headers_in.connection_type = STU_HTTP_CONNECTION_CLOSE;
	}

	return STU_OK;
}

static stu_int_t
stu_http_process_content_length(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_int_t         length;

	stu_http_process_unique_header_line(r, h, offset);

	length = atol((const char *) h->value.data);
	r->headers_in.content_length_n = length;

	return stu_http_process_header_line(r, h, offset);
}

static stu_int_t
stu_http_process_sec_websocket_key(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_table_elt_t *e;
	stu_sha1_t       sha1;
	stu_str_t        sha1_signed;

	stu_http_process_unique_header_line(r, h, offset);

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

	if (stu_list_push(&r->headers_out.headers, h, sizeof(stu_table_elt_t)) == STU_ERROR) {
		return STU_HTTP_INTERNAL_SERVER_ERROR;
	}

	return STU_OK;
}

static stu_int_t
stu_http_process_sec_websocket_key_for_safari(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_table_elt_t *e, *h1, *h2;
	stu_md5_t        md5;
	stu_str_t        md5_signed;
	u_char           s1[16], s2[16], key[16], *c, *s;
	stu_int_t        index, n, k1, k2;

	stu_http_process_unique_header_line(r, h, offset);

	if (r->headers_in.sec_websocket_key1 == NULL || r->headers_in.sec_websocket_key2 == NULL) {
		return STU_AGAIN;
	}

	e = stu_base_pcalloc(r->connection->pool, sizeof(stu_table_elt_t));
	if (e == NULL) {
		return STU_HTTP_INTERNAL_SERVER_ERROR;
	}

	h1 = r->headers_in.sec_websocket_key1;
	h2 = r->headers_in.sec_websocket_key2;
	stu_memzero(s1, 16);
	stu_memzero(s2, 16);

	for (index = 0, c = h1->value.data, s = s1, n = 0; index < h1->value.len; index++, c++) {
		if (*c >= '0' && *c <= '9') {
			*s++ = *c;
		} else if (*c == ' ') {
			n++;
		}
	}
	k1 = atoi((const char *) s1) / n;

	for (index = 0, c = h2->value.data, s = s2, n = 0; index < h2->value.len; index++, c++) {
		if (*c >= '0' && *c <= '9') {
			*s++ = *c;
		} else if (*c == ' ') {
			n++;
		}
	}
	k2 = atoi((const char *) s2) / n;

	key[0] = k1 >> 24;
	key[1] = k1 >> 16;
	key[2] = k1 >> 8;
	key[3] = k1;
	key[4] = k2 >> 24;
	key[5] = k2 >> 16;
	key[6] = k2 >> 8;
	key[7] = k2;

	e->key.data = STU_HTTP_HEADER_SEC_WEBSOCKET_ACCEPT.data;
	e->key.len = STU_HTTP_HEADER_SEC_WEBSOCKET_ACCEPT.len;

	md5_signed.len = 16;
	md5_signed.data = stu_base_pcalloc(r->connection->pool, md5_signed.len + 1);

	e->value.len = 16;
	e->value.data = stu_base_pcalloc(r->connection->pool, e->value.len + 1);

	if (md5_signed.data == NULL || e->value.data == NULL) {
		return STU_HTTP_INTERNAL_SERVER_ERROR;
	}

	stu_md5_init(&md5);
	stu_md5_update(&md5, key, 16);
	stu_md5_final(md5_signed.data, &md5);

	r->headers_out.sec_websocket_accept = e;

	if (stu_list_push(&r->headers_out.headers, e, sizeof(stu_table_elt_t)) == STU_ERROR) {
		return STU_HTTP_INTERNAL_SERVER_ERROR;
	}

	return STU_OK;
}

static stu_int_t
stu_http_process_sec_websocket_protocol(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset) {
	stu_table_elt_t *e;

	stu_http_process_unique_header_line(r, h, offset);

	e = stu_base_pcalloc(r->connection->pool, sizeof(stu_table_elt_t));
	if (e == NULL) {
		return STU_HTTP_INTERNAL_SERVER_ERROR;
	}

	e->key.data = STU_HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL.data;
	e->key.len = STU_HTTP_HEADER_SEC_WEBSOCKET_PROTOCOL.len;

	e->value.data = stu_base_pcalloc(r->connection->pool, h->value.len + 1);
	e->value.len = h->value.len;

	memcpy(e->value.data, h->value.data, h->value.len);

	r->headers_out.sec_websocket_protocol = e;

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
			"previous value => \"%s: %s\"", h->key.data, h->value.data, (*ph)->key.data, (*ph)->value.data);

	return STU_HTTP_BAD_REQUEST;
}


void
stu_http_finalize_request(stu_http_request_t *r, stu_int_t rc) {
	stu_connection_t *c;

	r->headers_out.status = rc;
	c = r->connection;

	/*c->write->handler = stu_http_request_handler;
	if (stu_event_add(c->write, STU_WRITE_EVENT, STU_CLEAR_EVENT) == STU_ERROR) {
		stu_log_error(0, "Failed to add http client write event.");
		return;
	}*/

	stu_http_request_handler(&c->write);
}

static void
stu_http_request_handler(stu_event_t *wev) {
	stu_http_request_t *r;
	stu_connection_t   *c;
	stu_channel_t      *ch;
	stu_buf_t          *buf;
	stu_table_elt_t    *accept;
	stu_table_elt_t    *protocol;
	stu_int_t           n;

	c = (stu_connection_t *) wev->data;

	//stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) -1) {
		return;
	}

	stu_event_del(&c->write, STU_WRITE_EVENT, 0);

	buf = &c->buffer;
	buf->last = buf->start;
	stu_memzero(buf->start, buf->end - buf->start);

	r = (stu_http_request_t *) c->data;
	accept = r->headers_out.sec_websocket_accept;
	protocol = r->headers_out.sec_websocket_protocol;

	if (r->headers_out.status == STU_HTTP_SWITCHING_PROTOCOLS) {
		buf->last = stu_sprintf(buf->last, "HTTP/1.1 101 Switching Protocols" CRLF);
		buf->last = stu_sprintf(buf->last, "Server: " __NAME "/" __VERSION CRLF);
		buf->last = stu_sprintf(buf->last, "Upgrade: websocket" CRLF);
		buf->last = stu_sprintf(buf->last, "Connection: upgrade" CRLF);
		buf->last = stu_strncpy(buf->last, accept->key.data, accept->key.len);
		buf->last = stu_sprintf(buf->last, ": ");
		buf->last = stu_strncpy(buf->last, accept->value.data, accept->value.len);
		buf->last = stu_sprintf(buf->last, CRLF);
		if (protocol) {
			buf->last = stu_strncpy(buf->last, protocol->key.data, protocol->key.len);
			buf->last = stu_sprintf(buf->last, ": ");
			buf->last = stu_strncpy(buf->last, protocol->value.data, protocol->value.len);
			buf->last = stu_sprintf(buf->last, CRLF);
		}
		buf->last = stu_sprintf(buf->last, CRLF);
	} else {
		buf->last = stu_sprintf(buf->last, "HTTP/1.1 400 Bad Request" CRLF);
		buf->last = stu_sprintf(buf->last, "Server: " __NAME "/" __VERSION CRLF);
		buf->last = stu_sprintf(buf->last, "Content-type: text/html" CRLF);
		buf->last = stu_sprintf(buf->last, "Content-length: %d" CRLF CRLF, stu_strlen(__NAME "/" __VERSION "\n"));
		buf->last = stu_sprintf(buf->last, __NAME "/" __VERSION "\n");
	}

	n = send(c->fd, buf->start, buf->last - buf->start, 0);
	if (n == -1) {
		stu_log_debug(4, "Failed to send data: fd=%d.", c->fd);
		goto failed;
	}

	stu_log_debug(4, "sent: fd=%d, bytes=%d.", c->fd, n);//str=\n%s, buf->start

	if (r->headers_out.status == STU_HTTP_SWITCHING_PROTOCOLS) {
		if (stu_http_switch_protocol(r) == STU_ERROR) {
			stu_log_error(0, "Failed to switch protocol: fd=%d.", c->fd);
			goto failed;
		}
	}

	return;

failed:

	c->read.active = 0;
	stu_event_del(&c->read, STU_READ_EVENT, 0);

	ch = c->user.channel;
	stu_channel_remove(ch, c);
	stu_http_close_connection(c);

//done:

	//stu_spin_unlock(&c->lock);
}

static stu_int_t
stu_http_switch_protocol(stu_http_request_t *r) {
	stu_connection_t *c;

	c = r->connection;

	stu_base_pool_reset(c->pool);

	c->buffer.start = c->buffer.last =c->buffer.end = NULL;
	c->data = NULL;

	c->read.handler = stu_websocket_wait_request_handler;
	c->write.handler = stu_websocket_request_handler;

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

