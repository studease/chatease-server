/*
 * stu_upstream_ident.c
 *
 *  Created on: 2017-3-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_list_t *stu_ident_upstream;

extern stu_cycle_t *stu_cycle;
extern stu_hash_t   stu_http_upstream_headers_in_hash;

static const stu_str_t  STU_UPSTREAM_IDENT_REQUEST = stu_string(

		"GET /websocket/data/userinfo.json?channel=%s&token=%s HTTP/1.1" CRLF
		"Host: localhost" CRLF
		"User-Agent: " __NAME "/" __VERSION CRLF
		"Accept: text/html" CRLF
		"Accept-Language: zh-CN,zh;q=0.8" CRLF
		"Connection: keep-alive" CRLF CRLF
		/*
		"POST /live/method=httpChatRoom HTTP/1.1" CRLF
		"Host: www.qcus.cn" CRLF
		"User-Agent: " __NAME "/" __VERSION CRLF
		"Accept: text/html" CRLF
		"Accept-Language: zh-CN,zh;q=0.8" CRLF
		"Content-Type: application/json" CRLF
		"Content-Length: %ld" CRLF
		"Connection: keep-alive" CRLF CRLF
		"{\"channel\":\"%s\",\"token\":\"%s\"}"
		 */
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
	u->process_response(c);

	u->peer.state = STU_UPSTREAM_PEER_COMPLETED;

	goto done;

failed:

	stu_upstream_cleanup(c);

	u->peer.connection = NULL;
	u->peer.state = STU_UPSTREAM_PEER_IDLE;

done:

	stu_spin_unlock(&c->lock);
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
		//stu_log_error(0, "Failed to analyze upstream ident response.");
		u->finalize_handler(c, STU_HTTP_INTERNAL_SERVER_ERROR);
		return STU_ERROR;
	}

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


stu_int_t
stu_upstream_ident_analyze_response(stu_connection_t *c) {
	stu_upstream_t     *u;
	stu_connection_t   *pc;
	stu_http_request_t *r, *pr;
	stu_uint_t          kh;
	stu_str_t          *cid, *uid, *uname, *channel_id;
	u_char             *data, temp[STU_HTTP_REQUEST_DEFAULT_SIZE];
	stu_channel_t      *ch;
	stu_json_t         *idt, *sta, *idchannel, *idcid, *idcstate, *iduser, *iduid, *iduname, *idurole;
	stu_json_t         *res, *raw, *rschannel, *rsctotal, *rsuser;

	u = c->upstream;
	pc = u->peer.connection;
	pr = (stu_http_request_t *) pc->data;
	r = (stu_http_request_t *) c->data;

	if (pr->headers_out.status != STU_HTTP_OK) {
		stu_log_error(0, "Failed to load ident data: status=%ld.", pr->headers_out.status);
		return STU_ERROR;
	}

	// parse JSON string
	idt = stu_json_parse(pc->buffer.last, pr->headers_out.content_length_n);
	if (idt == NULL) {
		stu_log_error(0, "Failed to parse ident response.");
		return STU_ERROR;
	}

	sta = stu_json_get_object_item_by(idt, &STU_PROTOCOL_STATUS);
	if (sta == NULL || sta->type != STU_JSON_TYPE_BOOLEAN) {
		stu_log_error(0, "Failed to analyze ident response: item status not found, fd=%d.", c->fd);
		goto failed;
	}

	if (sta->value == FALSE) {
		stu_log_error(0, "Access denied while joining channel, fd=%d.", c->fd);
		goto failed;
	}

	idchannel = stu_json_get_object_item_by(idt, &STU_PROTOCOL_CHANNEL);
	iduser = stu_json_get_object_item_by(idt, &STU_PROTOCOL_USER);
	if (idchannel == NULL || idchannel->type != STU_JSON_TYPE_OBJECT
			|| iduser == NULL || iduser->type != STU_JSON_TYPE_OBJECT) {
		stu_log_error(0, "Failed to analyze ident response: necessary item[s] not found.");
		goto failed;
	}

	idcid = stu_json_get_object_item_by(idchannel, &STU_PROTOCOL_ID);
	idcstate = stu_json_get_object_item_by(idchannel, &STU_PROTOCOL_STATE);

	iduid = stu_json_get_object_item_by(iduser, &STU_PROTOCOL_ID);
	iduname = stu_json_get_object_item_by(iduser, &STU_PROTOCOL_NAME);
	idurole = stu_json_get_object_item_by(iduser, &STU_PROTOCOL_ROLE);

	if (idcid == NULL || idcid->type != STU_JSON_TYPE_STRING
			|| idcstate == NULL || idcstate->type != STU_JSON_TYPE_NUMBER
			|| iduid == NULL || iduid->type != STU_JSON_TYPE_STRING
			|| iduname == NULL || iduname->type != STU_JSON_TYPE_STRING
			|| idurole == NULL || idurole->type != STU_JSON_TYPE_NUMBER) {
		stu_log_error(0, "Failed to analyze ident response: necessary item[s] not found.");
		goto failed;
	}

	// get channel ID
	channel_id = &r->target;

	cid = (stu_str_t *) idcid->value;
	if (stu_strncmp(cid->data, channel_id->data, channel_id->len) != 0) {
		stu_log_error(0, "Failed to analyze ident response: channel (%s != %s) not match.", cid->data, channel_id->data);
		goto failed;
	}

	// reset user info
	uid = (stu_str_t *) iduid->value;
	c->user.id = atol((const char *) uid->data);
	stu_strncpy(c->user.strid.data, uid->data, uid->len);
	c->user.strid.len = uid->len;

	uname = (stu_str_t *) iduname->value;
	c->user.name.data = stu_base_pcalloc(c->pool, uname->len + 1);
	if (c->user.name.data == NULL) {
		stu_log_error(0, "Failed to pcalloc memory for user name, fd=%d.", c->fd);
		goto failed;
	}
	stu_strncpy(c->user.name.data, uname->data, uname->len);
	c->user.name.len = uname->len;

	c->user.role = *(stu_double_t *) idurole->value;

	// insert user into channel
	kh = stu_hash_key_lc(cid->data, cid->len);

	stu_spin_lock(&stu_cycle->channels.lock);

	ch = stu_hash_find_locked(&stu_cycle->channels, kh, cid->data, cid->len);
	if (ch == NULL) {
		stu_log_debug(4, "channel \"%s\" not found: kh=%lu, i=%lu, len=%lu.",
				cid->data, kh, kh % stu_cycle->channels.size, stu_cycle->channels.length);

		ch = stu_slab_calloc(stu_cycle->slab_pool, sizeof(stu_channel_t));
		if (ch == NULL) {
			stu_log_error(0, "Failed to alloc new channel.");
			stu_spin_unlock(&stu_cycle->channels.lock);
			goto failed;
		}

		if (stu_channel_init(ch, cid) == STU_ERROR) {
			stu_log_error(0, "Failed to init channel.");
			stu_spin_unlock(&stu_cycle->channels.lock);
			goto failed;
		}

		if (stu_hash_init(&ch->userlist, NULL, STU_MAX_USER_N, stu_cycle->slab_pool,
				(stu_hash_palloc_pt) stu_slab_calloc, (stu_hash_free_pt) stu_slab_free) == STU_ERROR) {
			stu_log_error(0, "Failed to init userlist.");
			stu_spin_unlock(&stu_cycle->channels.lock);
			goto failed;
		}

		if (stu_hash_insert_locked(&stu_cycle->channels, cid, ch, STU_HASH_LOWCASE|STU_HASH_REPLACE) == STU_ERROR) {
			stu_log_error(0, "Failed to insert channel.");
			stu_spin_unlock(&stu_cycle->channels.lock);
			goto failed;
		}

		stu_log_debug(4, "new channel \"%s\": kh=%lu, total=%lu.",
				ch->id.data, kh, stu_atomic_read(&stu_cycle->channels.length));
	}

	stu_spin_unlock(&stu_cycle->channels.lock);

	if (stu_channel_insert(ch, c) == STU_ERROR) {
		stu_log_error(0, "Failed to insert connection.");
		goto failed;
	}

	c->user.channel = ch;

	// create ident response
	res = stu_json_create_object(NULL);
	raw = stu_json_create_string(&STU_PROTOCOL_RAW, STU_PROTOCOL_RAWS_IDENT.data, STU_PROTOCOL_RAWS_IDENT.len);
	rschannel = stu_json_duplicate(idchannel, TRUE);
	rsuser = stu_json_duplicate(iduser, TRUE);

	rsctotal = stu_json_create_number(&STU_PROTOCOL_TOTAL, (stu_double_t) ch->userlist.length);

	stu_json_add_item_to_object(rschannel, rsctotal);

	stu_json_add_item_to_object(res, raw);
	stu_json_add_item_to_object(res, rschannel);
	stu_json_add_item_to_object(res, rsuser);

	stu_memzero(temp, STU_HTTP_REQUEST_DEFAULT_SIZE);
	data = stu_json_stringify(res, (u_char *) temp);

	stu_json_delete(idt);
	stu_json_delete(res);

	r->response_body.start = temp;
	r->response_body.end = r->response_body.last = data;

	u->finalize_handler(c, STU_HTTP_SWITCHING_PROTOCOLS);

	return STU_OK;

failed:

	stu_json_delete(idt);

	return STU_ERROR;
}


void
stu_upstream_ident_finalize_handler(stu_connection_t *c, stu_int_t rc) {
	stu_http_request_t *r;
	stu_upstream_t     *u;

	r = (stu_http_request_t *) c->data;
	u = c->upstream;

	stu_upstream_cleanup(c);

	u->peer.connection = NULL;
	u->peer.state = STU_UPSTREAM_PEER_IDLE;

	stu_http_finalize_request(r, rc);
}


void
stu_upstream_ident_write_handler(stu_event_t *ev) {
	stu_connection_t   *c, *pc;
	stu_http_request_t *r;
	stu_upstream_t     *u;
	u_char             *data, temp[STU_HTTP_REQUEST_DEFAULT_SIZE], channel_id[8], tokenstr[STU_UPSTREAM_IDENT_TOKEN_MAX_LEN];
	stu_str_t           token;
	stu_int_t           n;

	c = (stu_connection_t *) ev->data;
	if (c->fd == (stu_socket_t) STU_SOCKET_INVALID || c->upstream == NULL) {
		return;
	}

	r = (stu_http_request_t *) c->data;
	u = c->upstream;
	pc = u->peer.connection;

	// Lock pc rather than c
	stu_spin_lock(&c->lock);

	if (pc == NULL || pc->fd == (stu_socket_t) STU_SOCKET_INVALID) {
		goto done;
	}

	if (u->peer.state >= STU_UPSTREAM_PEER_LOADING) {
		stu_log_debug(5, "upstream request already sent.");
		goto done;
	}

	stu_strncpy(channel_id, r->target.data, r->target.len);

	stu_str_null(&token);
	if (stu_http_arg(r, (u_char *) "token", 5, &token) != STU_OK) {
		stu_log_debug(4, "\"token\" not found while sending upstream %s: fd=%d.", u->server->name.data, pc->fd);
		*tokenstr = '\0';
	}
	stu_strncpy(tokenstr, token.data, token.len);

	stu_memzero(temp, STU_HTTP_REQUEST_DEFAULT_SIZE);
	data = stu_sprintf(temp, (const char *) STU_UPSTREAM_IDENT_REQUEST.data, channel_id, tokenstr);
	//data = stu_sprintf(temp, (const char *) STU_UPSTREAM_IDENT_REQUEST.data, 25 + stu_strlen(channel_id) + stu_strlen(tokenstr), channel_id, tokenstr);

	n = send(pc->fd, temp, data - temp, 0);
	if (n == -1) {
		stu_log_error(stu_errno, "Failed to send ident request, c->fd=%d, u->fd=%d.", c->fd, pc->fd);
		goto failed;
	}

	stu_log_debug(4, "sent to upstream %s: fd=%d, bytes=%d.", u->server->name.data, pc->fd, n);

	u->peer.state = STU_UPSTREAM_PEER_LOADING;

	ev->data = pc;
	stu_epoll_del_event(&pc->write, STU_WRITE_EVENT);
	ev->active = FALSE;

	goto done;

failed:

	stu_upstream_cleanup(c);

	u->peer.connection = NULL;
	u->peer.state = STU_UPSTREAM_PEER_IDLE;

done:

	stu_spin_unlock(&c->lock);
}
