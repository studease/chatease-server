/*
 * stu_hash.c
 *
 *  Created on: 2016-9-28
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_uint_t
stu_hash_key(u_char *data, size_t len) {
	stu_uint_t  i, key;

	key = 0;

	for (i = 0; i < len; i++) {
		key = stu_hash(key, data[i]);
	}

	return key;
}

stu_int_t
stu_hash_init(stu_hash_t *hash, stu_uint_t size, stu_slab_pool_t *pool) {
	stu_hash_elt_t **buckets;

	buckets = stu_slab_alloc(pool, size * sizeof(stu_hash_elt_t *));
	if (buckets == NULL) {
		stu_log_error(0, "Failed to alloc buckets of hash.");
		return STU_ERROR;
	}

	stu_spinlock_init(&hash->lock);
	hash->pool = pool;

	hash->buckets = buckets;
	hash->size = size;
	hash->length = 0;

	return STU_OK;
}

stu_int_t
stu_hash_insert(stu_hash_t *hash, stu_str_t *key, void *value) {
	stu_uint_t      kh, k;
	stu_hash_elt_t *elts, *e, *elt;
	stu_queue_t    *q;

	kh = stu_hash_key(key->data, key->len);
	k = kh % hash->size;

	elts = hash->buckets[k];
	if (elts == NULL) {
		elts = stu_slab_alloc(hash->pool, sizeof(stu_hash_elt_t));
		stu_queue_init(&elts->queue);
		hash->buckets[k] = elts;
	}

	for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, queue);
		if (e->key_hash != kh || e->key.len != key->len) {
			continue;
		}
		if (strncmp((const char *) e->key.data, (const char *) key->data, key->len) == 0) {
			e->value = value;
			return STU_OK;
		}
	}

	elt = stu_slab_alloc(hash->pool, sizeof(stu_hash_elt_t));
	stu_queue_add(&elts->queue, &elt->queue);

	elt->key.data = stu_slab_alloc(hash->pool, key->len + 1);
	stu_strncpy(elt->key.data, key->data, key->len);
	elt->key.len = key->len;

	elt->key_hash = kh;
	elt->value = value;

	return STU_OK;
}

void *
stu_hash_find(stu_hash_t *hash, stu_str_t *key) {
	stu_uint_t      kh, k;
	stu_hash_elt_t *elts, *e;
	stu_queue_t    *q;

	kh = stu_hash_key(key->data, key->len);
	k = kh % hash->size;

	elts = hash->buckets[k];
	if (elts == NULL) {
		return NULL;
	}

	for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, queue);
		if (e->key_hash != kh || e->key.len != key->len) {
			continue;
		}
		if (strncmp((const char *) e->key.data, (const char *) key->data, key->len) == 0) {
			return e->value;
		}
	}

	return NULL;
}

void
stu_hash_remove(stu_hash_t *hash, stu_str_t *key) {
	stu_uint_t      kh, k;
	stu_hash_elt_t *elts, *e;
	stu_queue_t    *q;

	kh = stu_hash_key(key->data, key->len);
	k = kh % hash->size;

	elts = hash->buckets[k];
	if (elts == NULL) {
		return;
	}

	for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, queue);
		if (e->key_hash != kh || e->key.len != key->len) {
			continue;
		}
		if (strncmp((const char *) e->key.data, (const char *) key->data, key->len) == 0) {
			stu_spin_lock(&hash->lock);
			stu_slab_free_locked(hash->pool, e->key.data);
			stu_slab_free_locked(hash->pool, e);
			stu_spin_unlock(&hash->lock);
			break;
		}
	}
}

