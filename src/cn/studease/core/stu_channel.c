/*
 * stu_channel.c
 *
 *  Created on: 2016-12-5
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_str_t           STU_CHANNEL_TIMER_PUSH_USERS = stu_string("push_users");
stu_str_t           STU_CHANNEL_TIMER_PUSH_STATUS = stu_string("push_status");

extern stu_cycle_t *stu_cycle;
extern stu_str_t    STU_HTTP_UPSTREAM_STATUS;

static void       stu_channel_remove_user_locked(stu_channel_t *ch, stu_connection_t *c);
static void       stu_channel_push_users(stu_str_t *key, void *value);
static stu_int_t  stu_channel_push_status_generate_request(stu_connection_t *c);
static stu_int_t  stu_channel_push_status_analyze_response(stu_connection_t *c);
static void       stu_channel_push_status_finalize_handler(stu_connection_t *c, stu_int_t rc);


stu_int_t
stu_channel_insert(stu_str_t *id, stu_connection_t *c) {
	stu_int_t      rc;
	stu_uint_t     hk;
	stu_channel_t *ch;

	rc = STU_ERROR;
	hk = stu_hash_key_lc(id->data, id->len);

	stu_mutex_lock(&stu_cycle->channels.lock);

	ch = stu_hash_find_locked(&stu_cycle->channels, hk, id->data, id->len);
	if (ch == NULL) {
		stu_log_debug(4, "channel \"%s\" not found: kh=%lu, i=%lu, len=%lu.",
				id->data, hk, hk % stu_cycle->channels.size, stu_cycle->channels.length);

		ch = stu_calloc(sizeof(stu_channel_t));
		if (ch == NULL) {
			stu_log_error(0, "Failed to slab_calloc() new channel.");
			goto failed;
		}

		ch->id.data = stu_calloc(id->len + 1);
		if (ch->id.data == NULL) {
			stu_log_error(0, "Failed to slab_calloc() channel id.");
			goto failed;
		}
		ch->id.len = id->len;
		memcpy(ch->id.data, id->data, id->len);

		if (stu_hash_init(&ch->userlist, NULL, STU_USER_MAXIMUM,
				(stu_hash_palloc_pt) stu_calloc, (stu_hash_free_pt) stu_free) == STU_ERROR) {
			stu_log_error(0, "Failed to init userlist.");
			goto failed;
		}

		if (stu_hash_insert_locked(&stu_cycle->channels, id, ch, STU_HASH_LOWCASE|STU_HASH_REPLACE) == STU_ERROR) {
			stu_log_error(0, "Failed to insert channel.");
			goto failed;
		}

		stu_log_debug(4, "new channel \"%s\": kh=%lu, total=%lu.",
				ch->id.data, hk, stu_cycle->channels.length);
	}

	stu_mutex_lock(&ch->userlist.lock);
	rc = stu_channel_insert_locked(ch, c);
	stu_mutex_unlock(&ch->userlist.lock);

failed:

	stu_mutex_unlock(&stu_cycle->channels.lock);

	return rc;
}

stu_int_t
stu_channel_insert_locked(stu_channel_t *ch, stu_connection_t *c) {
	if (stu_hash_insert_locked(&ch->userlist, &c->user.id, c, STU_HASH_LOWCASE) == STU_ERROR) {
		stu_log_error(0, "Failed to insert user \"%s\" into channel \"%s\", total=%lu.", c->user.id.data, ch->id.data, ch->userlist.length);
		return STU_ERROR;
	}

	c->user.channel = ch;

	stu_log_debug(4, "Inserted user \"%s\" into channel \"%s\", total=%lu.", c->user.id.data, ch->id.data, ch->userlist.length);

	return STU_OK;
}

void
stu_channel_remove(stu_channel_t *ch, stu_connection_t *c) {
	stu_mutex_lock(&ch->userlist.lock);
	stu_channel_remove_locked(ch, c);
	stu_mutex_unlock(&ch->userlist.lock);
}

void
stu_channel_remove_locked(stu_channel_t *ch, stu_connection_t *c) {
	stu_str_t     *key;
	stu_uint_t     kh;

	stu_channel_remove_user_locked(ch, c);

	if (ch->userlist.length == 0) {
		if (ch->userlist.free) {
			ch->userlist.free(ch->userlist.buckets);
		}
		ch->userlist.buckets = NULL;

		key = &ch->id;
		kh = stu_hash_key_lc(key->data, key->len);

		stu_hash_remove(&stu_cycle->channels, kh, key->data, key->len);

		stu_free(key->data);
		stu_free(ch);

		stu_log_debug(4, "removed channel \"%s\", total=%lu.", key->data, ch->userlist.length);
	}
}

static void
stu_channel_remove_user_locked(stu_channel_t *ch, stu_connection_t *c) {
	stu_uint_t        kh, i;
	stu_hash_t       *hash;
	stu_hash_elt_t   *elts, *e;
	stu_queue_t      *q;
	stu_connection_t *vc;

	hash = &ch->userlist;
	kh = stu_hash_key_lc(c->user.id.data, c->user.id.len);
	i = kh % hash->size;

	elts = hash->buckets[i];
	if (elts == NULL) {
		stu_log_error(0, "Failed to remove from hash: key=%lu, i=%lu, name=%s.", kh, i, c->user.id.data);
		return;
	}

	for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, queue);
		vc = (stu_connection_t *) e->value;
		if (e->key_hash != kh || e->key.len != c->user.id.len || vc->fd != c->fd) {
			continue;
		}
		if (stu_strncmp(e->key.data, c->user.id.data, c->user.id.len) == 0) {
			stu_queue_remove(&e->queue);
			stu_queue_remove(&e->q);

			if (hash->free) {
				hash->free(e->key.data);
				hash->free(e);
			}

			hash->length--;

			stu_log_debug(1, "Removed %p from hash: key=%lu, i=%lu, name=%s.", e->value, kh, i, c->user.id.data);

			break;
		}
	}

	if (elts->queue.next == stu_queue_sentinel(&elts->queue)) {
		if (hash->free) {
			hash->free(elts);
		}

		hash->buckets[i] = NULL;

		//stu_log_debug(1, "Removed sentinel from hash: key=%lu, i=%lu, name=%s.", key, i, name);
	}

	stu_log_debug(4, "removed user \"%s\" from channel \"%s\", total=%lu.", c->user.id.data, ch->id.data, ch->userlist.length);
}


stu_int_t
stu_channel_add_timers() {
	stu_int_t         rc;
	stu_connection_t *c;
	stu_uint_t        hk;

	rc = STU_ERROR;

	stu_mutex_lock(&stu_cycle->timers.lock);

	if (stu_cycle->config.push_users) {
		hk = stu_hash_key_lc(STU_CHANNEL_TIMER_PUSH_USERS.data, STU_CHANNEL_TIMER_PUSH_USERS.len);

		c = stu_hash_find_locked(&stu_cycle->timers, hk, STU_CHANNEL_TIMER_PUSH_USERS.data, STU_CHANNEL_TIMER_PUSH_USERS.len);
		if (c == NULL) {
			stu_log_debug(4, "timer \"%s\" not found: kh=%lu, i=%lu, len=%lu.",
					STU_CHANNEL_TIMER_PUSH_USERS.data, hk, hk % stu_cycle->timers.size, stu_cycle->timers.length);

			c = stu_connection_get((stu_socket_t) -2);
			if (c == NULL) {
				stu_log_error(0, "Failed to get connection for pushing users.");
				goto done;
			}

			if (stu_hash_insert_locked(&stu_cycle->timers, &STU_CHANNEL_TIMER_PUSH_USERS, c, STU_HASH_LOWCASE) ==STU_ERROR) {
				stu_log_error(0, "Failed to insert timer \"%s\", total=%lu.", STU_CHANNEL_TIMER_PUSH_USERS.data, stu_cycle->timers.length);
				goto done;
			}

			stu_log_debug(4, "new timer \"%s\": kh=%lu, total=%lu.",
					STU_CHANNEL_TIMER_PUSH_USERS.data, hk, stu_cycle->timers.length);
		}

		c->write.handler = stu_channel_push_users_handler;
		stu_timer_add(&c->write, stu_cycle->config.push_users_interval);
	}

	if (stu_cycle->config.push_status) {
		hk = stu_hash_key_lc(STU_CHANNEL_TIMER_PUSH_STATUS.data, STU_CHANNEL_TIMER_PUSH_STATUS.len);

		c = stu_hash_find_locked(&stu_cycle->timers, hk, STU_CHANNEL_TIMER_PUSH_STATUS.data, STU_CHANNEL_TIMER_PUSH_STATUS.len);
		if (c == NULL) {
			stu_log_debug(4, "timer \"%s\" not found: kh=%lu, i=%lu, len=%lu.",
					STU_CHANNEL_TIMER_PUSH_STATUS.data, hk, hk % stu_cycle->timers.size, stu_cycle->timers.length);

			c = stu_connection_get((stu_socket_t) -2);
			if (c == NULL) {
				stu_log_error(0, "Failed to get connection for pushing users.");
				goto done;
			}

			if (stu_hash_insert_locked(&stu_cycle->timers, &STU_CHANNEL_TIMER_PUSH_STATUS, c, STU_HASH_LOWCASE) ==STU_ERROR) {
				stu_log_error(0, "Failed to insert timer \"%s\", total=%lu.", STU_CHANNEL_TIMER_PUSH_STATUS.data, stu_cycle->timers.length);
				goto done;
			}

			stu_log_debug(4, "new timer \"%s\": kh=%lu, total=%lu.",
					STU_CHANNEL_TIMER_PUSH_STATUS.data, hk, stu_cycle->timers.length);
		}

		c->write.handler = stu_channel_push_status_handler;
		stu_timer_add(&c->write, stu_cycle->config.push_status_interval);
	}

	rc = STU_OK;

done:

	stu_mutex_unlock(&stu_cycle->timers.lock);

	return rc;
}

void
stu_channel_push_users_handler(stu_event_t *ev) {
	stu_connection_t *c;

	c = (stu_connection_t *) ev->data;

	stu_hash_foreach(&stu_cycle->channels, stu_channel_push_users);
	stu_timer_add_locked(&c->write, stu_cycle->config.push_users_interval);
}

static void
stu_channel_push_users(stu_str_t *key, void *value) {
	stu_channel_t    *ch;
	stu_list_elt_t   *elts;
	stu_hash_elt_t   *e;
	stu_queue_t      *q;
	stu_connection_t *c;
	stu_json_t       *res, *raw, *rschannel, *rscid, *rscstate, *rsctotal;
	u_char           *data, temp[STU_HTTP_REQUEST_DEFAULT_SIZE];
	stu_socket_t      fd;
	uint64_t          size;
	stu_int_t         extened, n;

	ch = (stu_channel_t *) value;

	stu_mutex_lock(&ch->userlist.lock);

	stu_log_debug(4, "broadcasting in channel \"%s\".", key->data);

	res = stu_json_create_object(NULL);
	raw = stu_json_create_string(&STU_PROTOCOL_RAW, STU_PROTOCOL_RAWS_USERS.data, STU_PROTOCOL_RAWS_USERS.len);

	rschannel = stu_json_create_object(&STU_PROTOCOL_CHANNEL);

	rscid = stu_json_create_string(&STU_PROTOCOL_ID, key->data, key->len);
	rscstate = stu_json_create_number(&STU_PROTOCOL_STATE, (stu_double_t) ch->state);
	rsctotal = stu_json_create_number(&STU_PROTOCOL_TOTAL, (stu_double_t) ch->userlist.length);

	stu_json_add_item_to_object(rschannel, rscid);
	stu_json_add_item_to_object(rschannel, rscstate);
	stu_json_add_item_to_object(rschannel, rsctotal);

	stu_json_add_item_to_object(res, raw);
	stu_json_add_item_to_object(res, rschannel);

	data = stu_json_stringify(res, (u_char *) temp + 10);
	*data = '\0';

	stu_json_delete(res);

	size = data - temp - 10;
	data = stu_websocket_encode_frame(STU_WEBSOCKET_OPCODE_BINARY, temp, size, &extened);

	elts = &ch->userlist.keys.elts;
	for (q = stu_queue_head(&elts->queue); q != NULL && q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, q);
		c = (stu_connection_t *) e->value;
		fd = stu_atomic_fetch(&c->fd);

		n = send(fd, data, size + 2 + extened, 0);
		if (n == -1) {
			//stu_log_error(stu_errno, "Failed to broadcast in channel \"%s\": , fd=%d.", id->data, fd);
			continue;
		}
	}

	stu_mutex_unlock(&ch->userlist.lock);
}

void
stu_channel_push_status_handler(stu_event_t *ev) {
	stu_connection_t *c;

	c = (stu_connection_t *) ev->data;

	if (stu_upstream_create(c, STU_HTTP_UPSTREAM_STATUS.data, STU_HTTP_UPSTREAM_STATUS.len) == STU_ERROR) {
		stu_log_error(0, "Failed to create http upstream \"status\".");
		return;
	}

	c->upstream->read_event_handler = stu_http_upstream_read_handler;
	c->upstream->write_event_handler = stu_http_upstream_write_handler;

	c->upstream->create_request_pt = stu_http_upstream_create_request;
	c->upstream->reinit_request_pt = stu_http_upstream_reinit_request;
	c->upstream->generate_request_pt = stu_channel_push_status_generate_request;
	c->upstream->process_response_pt = stu_http_upstream_process_response;
	c->upstream->analyze_response_pt = stu_channel_push_status_analyze_response;
	c->upstream->finalize_handler_pt = stu_channel_push_status_finalize_handler;
	c->upstream->cleanup_pt = stu_http_upstream_cleanup;

	if (stu_upstream_init(c) == STU_ERROR) {
		stu_log_error(0, "Failed to init upstream.");
	}
}

static stu_int_t
stu_channel_push_status_generate_request(stu_connection_t *c) {
	stu_upstream_t     *u;
	stu_connection_t   *pc;
	stu_http_request_t *r, *pr;
	stu_channel_t      *ch;
	stu_list_elt_t     *elts;
	stu_hash_elt_t     *e;
	stu_queue_t        *q;
	stu_json_t         *res, *rschannel, *rscstate, *rsctotal;
	u_char             *p;
	stu_table_elt_t    *h;
	stu_int_t           total;

	u = c->upstream;
	pc = u->peer.connection;
	pr = (stu_http_request_t *) pc->data;

	res = stu_json_create_object(NULL);
	total = 0;

	stu_mutex_lock(&stu_cycle->channels.lock);

	elts = &stu_cycle->channels.keys.elts;
	for (q = stu_queue_head(&elts->queue); q != NULL && q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, q);
		ch = (stu_channel_t *) e->value;

		stu_mutex_lock(&ch->userlist.lock);

		rschannel = stu_json_create_object(&e->key);

		rscstate = stu_json_create_number(&STU_PROTOCOL_STATE, (stu_double_t) ch->state);
		rsctotal = stu_json_create_number(&STU_PROTOCOL_TOTAL, (stu_double_t) ch->userlist.length);

		stu_json_add_item_to_object(rschannel, rscstate);
		stu_json_add_item_to_object(rschannel, rsctotal);

		stu_json_add_item_to_object(res, rschannel);

		total += ch->userlist.length;

		stu_mutex_unlock(&ch->userlist.lock);
	}

	stu_mutex_unlock(&stu_cycle->channels.lock);

	if (pr->request_body.start == NULL) {
		pr->request_body.start = stu_calloc(STU_HTTP_REQUEST_DEFAULT_SIZE);
		if (pr->request_body.start == NULL) {
			stu_log_error(0, "Failed to palloc() request body: fd=%d.", c->fd);
			stu_json_delete(res);
			return STU_ERROR;
		}

		pr->request_body.end = pr->request_body.start + STU_HTTP_REQUEST_DEFAULT_SIZE;
	}

	p = stu_json_stringify(res, pr->request_body.start);
	*p = '\0';

	pr->request_body.last = p;

	stu_json_delete(res);

	// fit to normal http connection
	if (c->data == NULL) {
		c->data = (void *) stu_http_create_request(c);
		if (c->data == NULL) {
			stu_log_error(0, "Failed to create http request for upstream %s.", u->server->name.data);
			return STU_ERROR;
		}

		r = (stu_http_request_t *) c->data;

		h = stu_calloc(sizeof(stu_table_elt_t));
		if (h == NULL) {
			stu_log_error(0, "Failed to pcalloc table elt \"Host\" for http upstream %s request.", u->server->name.data);
			return STU_ERROR;
		}

		h->key.data = stu_calloc(5);
		if (h->key.data == NULL) {
			return STU_ERROR;
		}
		stu_strncpy(h->key.data, (u_char *) "Host", 4);
		h->key.len = 4;

		h->hash = stu_hash_key_lc(h->key.data, h->key.len);

		h->value.data = stu_calloc(u->server->addr.name.len + 1);
		if (h->value.data == NULL) {
			return STU_ERROR;
		}
		stu_strncpy(h->value.data, u->server->addr.name.data, u->server->addr.name.len);
		h->value.len = u->server->addr.name.len;

		h->lowcase_key = stu_calloc(h->key.len + 1);
		if (h->lowcase_key == NULL) {
			return STU_ERROR;
		}
		stu_strlow(h->lowcase_key, h->key.data, h->key.len);
		h->lowcase_key[h->key.len] = '\0';

		if (stu_list_push(&r->headers_in.headers, h, sizeof(stu_table_elt_t)) == STU_ERROR) {
			return STU_ERROR;
		}

		r->headers_in.host = h;
	}

	stu_log("Generated push status request, total=%ld.", total);

	stu_http_upstream_generate_request(c);

	return STU_OK;
}

static stu_int_t
stu_channel_push_status_analyze_response(stu_connection_t *c) {
	stu_upstream_t     *u;
	stu_connection_t   *pc;
	stu_http_request_t *pr;

	u = c->upstream;
	pc = u->peer.connection;
	pr = (stu_http_request_t *) pc->data;

	if (pr->headers_out.status != STU_HTTP_OK) {
		stu_log_error(0, "Bad push status response: code=%lu.", pr->headers_out.status);
		return STU_ERROR;
	}

	stu_log("Push status done.");

	u->finalize_handler_pt(c, pr->headers_out.status);

	return STU_OK;
}

static void
stu_channel_push_status_finalize_handler(stu_connection_t *c, stu_int_t rc) {
	stu_upstream_t     *u;

	u = c->upstream;

	u->cleanup_pt(c);

	stu_timer_add_locked(&c->write, stu_cycle->config.push_status_interval);
}
