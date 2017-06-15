/*
 * stu_channel.c
 *
 *  Created on: 2016-12-5
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

extern stu_cycle_t *stu_cycle;

static void stu_channel_remove_user_locked(stu_channel_t *ch, stu_connection_t *c);


stu_int_t
stu_channel_insert(stu_str_t *id, stu_connection_t *c) {
	stu_int_t      rc;
	stu_uint_t     kh;
	stu_channel_t *ch;

	rc = STU_ERROR;
	kh = stu_hash_key_lc(id->data, id->len);

	stu_spin_lock(&stu_cycle->channels.lock);

	ch = stu_hash_find_locked(&stu_cycle->channels, kh, id->data, id->len);
	if (ch == NULL) {
		stu_log_debug(4, "channel \"%s\" not found: kh=%lu, i=%lu, len=%lu.",
				id->data, kh, kh % stu_cycle->channels.size, stu_cycle->channels.length);

		ch = stu_slab_calloc(stu_cycle->slab_pool, sizeof(stu_channel_t));
		if (ch == NULL) {
			stu_log_error(0, "Failed to slab_calloc() new channel.");
			goto failed;
		}

		ch->id.data = stu_slab_calloc(stu_cycle->slab_pool, id->len + 1);
		if (ch->id.data == NULL) {
			stu_log_error(0, "Failed to slab_calloc() channel id.");
			goto failed;
		}
		ch->id.len = id->len;
		memcpy(ch->id.data, id->data, id->len);

		if (stu_hash_init(&ch->userlist, NULL, STU_MAX_USER_N, stu_cycle->slab_pool,
				(stu_hash_palloc_pt) stu_slab_calloc, (stu_hash_free_pt) stu_slab_free) == STU_ERROR) {
			stu_log_error(0, "Failed to init userlist.");
			goto failed;
		}

		if (stu_hash_insert_locked(&stu_cycle->channels, id, ch, STU_HASH_LOWCASE|STU_HASH_REPLACE) == STU_ERROR) {
			stu_log_error(0, "Failed to insert channel.");
			goto failed;
		}

		stu_log_debug(4, "new channel \"%s\": kh=%lu, total=%lu.",
				ch->id.data, kh, stu_cycle->channels.length);
	}

	stu_spin_lock(&ch->userlist.lock);
	rc = stu_channel_insert_locked(ch, c);
	stu_spin_unlock(&ch->userlist.lock);

failed:

	stu_spin_unlock(&stu_cycle->channels.lock);

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
	stu_spin_lock(&ch->userlist.lock);
	stu_channel_remove_locked(ch, c);
	stu_spin_unlock(&ch->userlist.lock);
}

void
stu_channel_remove_locked(stu_channel_t *ch, stu_connection_t *c) {
	stu_str_t     *key;
	stu_uint_t     kh;

	stu_channel_remove_user_locked(ch, c);

	if (ch->userlist.length == 0) {
		if (ch->userlist.free) {
			ch->userlist.free(ch->userlist.pool, ch->userlist.buckets);
		}
		ch->userlist.buckets = NULL;

		key = &ch->id;
		kh = stu_hash_key_lc(key->data, key->len);

		stu_hash_remove(&stu_cycle->channels, kh, key->data, key->len);

		stu_slab_free(stu_cycle->slab_pool, key->data);
		stu_slab_free(stu_cycle->slab_pool, ch);

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
				hash->free(hash->pool, e->key.data);
				hash->free(hash->pool, e);
			}

			hash->length--;

			stu_log_debug(1, "Removed %p from hash: key=%lu, i=%lu, name=%s.", e->value, kh, i, c->user.id.data);

			break;
		}
	}

	if (elts->queue.next == stu_queue_sentinel(&elts->queue)) {
		if (hash->free) {
			hash->free(hash->pool, elts);
		}

		hash->buckets[i] = NULL;

		//stu_log_debug(1, "Removed sentinel from hash: key=%lu, i=%lu, name=%s.", key, i, name);
	}

	stu_log_debug(4, "removed user \"%s\" from channel \"%s\", total=%lu.", c->user.id.data, ch->id.data, ch->userlist.length);
}


void
stu_channel_broadcast(stu_str_t *id, void *ch) {
	stu_channel_t    *channel;
	stu_list_elt_t   *elts;
	stu_hash_elt_t   *e;
	stu_queue_t      *q;
	stu_connection_t *c;
	stu_json_t       *res, *raw, *rschannel, *rscid, *rscstate, *rsctotal;
	u_char           *data, temp[STU_CHANNEL_PUSH_USERS_DEFAULT_SIZE];
	stu_socket_t      fd;
	uint64_t          size;
	stu_int_t         extened, n;

	channel = (stu_channel_t *) ch;

	stu_log_debug(4, "broadcasting in channel \"%s\".", id->data);

	stu_spin_lock(&channel->userlist.lock);

	res = stu_json_create_object(NULL);
	raw = stu_json_create_string(&STU_PROTOCOL_RAW, STU_PROTOCOL_RAWS_USERS.data, STU_PROTOCOL_RAWS_USERS.len);

	rschannel = stu_json_create_object(&STU_PROTOCOL_CHANNEL);

	rscid = stu_json_create_string(&STU_PROTOCOL_ID, id->data, id->len);
	rscstate = stu_json_create_number(&STU_PROTOCOL_STATE, (stu_double_t) channel->state);
	rsctotal = stu_json_create_number(&STU_PROTOCOL_TOTAL, (stu_double_t) channel->userlist.length);

	stu_json_add_item_to_object(rschannel, rscid);
	stu_json_add_item_to_object(rschannel, rscstate);
	stu_json_add_item_to_object(rschannel, rsctotal);

	stu_json_add_item_to_object(res, raw);
	stu_json_add_item_to_object(res, rschannel);

	stu_memzero(temp, STU_CHANNEL_PUSH_USERS_DEFAULT_SIZE);
	data = stu_json_stringify(res, (u_char *) temp + 10);

	stu_json_delete(res);

	size = data - temp - 10;
	data = stu_websocket_encode_frame(STU_WEBSOCKET_OPCODE_BINARY, temp, size, &extened);

	elts = &channel->userlist.keys.elts;
	for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, q);
		c = (stu_connection_t *) e->value;
		fd = stu_atomic_fetch(&c->fd);

		n = send(fd, data, size + 2 + extened, 0);
		if (n == -1) {
			stu_log_error(stu_errno, "Failed to broadcast in channel \"%s\": , fd=%d.", id->data, fd);
			continue;
		}
	}

	stu_spin_unlock(&channel->userlist.lock);
}

