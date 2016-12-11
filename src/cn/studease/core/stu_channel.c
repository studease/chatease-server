/*
 * stu_channel.c
 *
 *  Created on: 2016-12-5
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

extern stu_cycle_t *stu_cycle;


stu_int_t
stu_channel_init(stu_channel_t *ch, stu_str_t *id) {
	ch->id.data = (u_char *) ch  + sizeof(stu_channel_t);
	memcpy(ch->id.data, id->data, id->len);
	ch->id.len = id->len;

	return STU_OK;
}

stu_int_t
stu_channel_insert(stu_channel_t *ch, stu_connection_t *c) {
	stu_int_t  rc;

	stu_spin_lock(&ch->userlist.lock);
	rc = stu_channel_insert_locked(ch, c);
	stu_spin_unlock(&ch->userlist.lock);

	return rc;
}

stu_int_t
stu_channel_insert_locked(stu_channel_t *ch, stu_connection_t *c) {
	stu_str_t *key;

	key = &c->user.strid;

	if (stu_hash_insert_locked(&ch->userlist, key, c, STU_HASH_LOWCASE_KEY) == STU_ERROR) {
		stu_log_error(0, "Failed to insert user(\"%s\") into channel(\"%s\"), total=%lu.", key->data, ch->id.data, ch->userlist.length);
		return STU_ERROR;
	}

	stu_log_debug(0, "Inserted user(\"%s\") into channel(\"%s\"), total=%lu.", key->data, ch->id.data, ch->userlist.length);

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

	key = &c->user.strid;
	kh = stu_hash_key_lc(key->data, key->len);

	stu_hash_remove_locked(&ch->userlist, kh, key->data, key->len);

	stu_log_debug(0, "removed user(\"%s\") from channel(\"%s\"), total=%lu.", key->data, ch->id.data, ch->userlist.length);

	if (ch->userlist.length == 0) {
		if (ch->userlist.free) {
			ch->userlist.free(ch->userlist.pool, ch->userlist.buckets);
		}
		ch->userlist.buckets = NULL;

		key = &ch->id;
		kh = stu_hash_key_lc(key->data, key->len);

		stu_hash_remove(&stu_cycle->channels, kh, key->data, key->len);

		stu_slab_free(stu_cycle->slab_pool, ch);

		stu_log_debug(0, "removed channel(\"%s\") from channel(\"%s\"), total=%lu.", key->data, ch->id.data, ch->userlist.length);
	}
}

