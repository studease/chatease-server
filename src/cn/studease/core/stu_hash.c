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

stu_uint_t
stu_hash_key_lc(u_char *data, size_t len) {
	stu_uint_t  i, key;

	key = 0;

	for (i = 0; i < len; i++) {
		key = stu_hash(key, stu_tolower(data[i]));
	}

	return key;
}

stu_int_t
stu_hash_init(stu_hash_t *hash, stu_hash_elt_t **buckets, stu_uint_t size, void *pool, stu_hash_palloc_pt palloc, stu_hash_free_pt free) {
	stu_hash_elt_t **b;

	b = buckets;
	if (b == NULL) {
		b = palloc(pool, size * sizeof(stu_hash_elt_t *));
		if (b == NULL) {
			stu_log_error(0, "Failed to alloc buckets of hash.");
			return STU_ERROR;
		}
	}

	stu_spinlock_init(&hash->lock);

	stu_list_init(&hash->keys, pool);

	hash->buckets = b;
	hash->size = size;
	hash->length = 0;

	hash->pool = pool;
	hash->palloc = palloc;
	hash->free = free;

	return STU_OK;
}

stu_int_t
stu_hash_insert(stu_hash_t *hash, stu_str_t *key, void *value, stu_uint_t flags) {
	stu_int_t  rc;

	stu_spin_lock(&hash->lock);
	rc = stu_hash_insert_locked(hash, key, value, flags);
	stu_spin_unlock(&hash->lock);

	return rc;
}

stu_int_t
stu_hash_insert_locked(stu_hash_t *hash, stu_str_t *key, void *value, stu_uint_t flags) {
	stu_uint_t      kh, k;
	stu_hash_elt_t *elts, *e, *elt;
	stu_queue_t    *q;

	if (flags) {
		kh = stu_hash_key_lc(key->data, key->len);
	} else {
		kh = stu_hash_key(key->data, key->len);
	}
	k = kh % hash->size;

	elts = hash->buckets[k];
	if (elts == NULL) {
		elts = hash->palloc(hash->pool, sizeof(stu_hash_elt_t));
		if (elts == NULL) {
			return STU_ERROR;
		}

		stu_queue_init(&elts->queue);
		hash->buckets[k] = elts;
	}

	for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, queue);
		if (e->key_hash != kh || e->key.len != key->len) {
			continue;
		}
		if (stu_strncmp(e->key.data, key->data, key->len) == 0) {
			e->value = value;
			return STU_OK;
		}
	}

	elt = hash->palloc(hash->pool, sizeof(stu_hash_elt_t));
	if (elt == NULL) {
		return STU_ERROR;
	}
	stu_queue_insert_tail(&elts->queue, &elt->queue);
	hash->length++;

	elt->key.data = hash->palloc(hash->pool, key->len + 1);
	if (elt->key.data == NULL) {
		return STU_ERROR;
	}

	stu_strncpy(elt->key.data, key->data, key->len);
	elt->key.len = key->len;

	elt->key_hash = kh;
	elt->value = value;

	stu_queue_init(&elt->q);
	stu_queue_insert_tail(&hash->keys.elts.queue, &elt->q);

	return STU_OK;
}

void *
stu_hash_find(stu_hash_t *hash, stu_uint_t key, u_char *name, size_t len) {
	void *v;

	stu_spin_lock(&hash->lock);
	v = stu_hash_find_locked(hash, key, name, len);
	stu_spin_unlock(&hash->lock);

	return v;
}

void *
stu_hash_find_locked(stu_hash_t *hash, stu_uint_t key, u_char *name, size_t len) {
	stu_uint_t      i;
	stu_hash_elt_t *elts, *e;
	stu_queue_t    *q;

	i = key % hash->size;

	elts = hash->buckets[i];
	if (elts == NULL) {
		return NULL;
	}

	for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, queue);
		if (e->key_hash != key || e->key.len != len) {
			continue;
		}
		if (stu_strncmp(e->key.data, name, len) == 0) {
			return e->value;
		}
	}

	return NULL;
}

void
stu_hash_remove(stu_hash_t *hash, stu_uint_t key, u_char *name, size_t len) {
	stu_spin_lock(&hash->lock);
	stu_hash_remove_locked(hash, key, name, len);
	stu_spin_unlock(&hash->lock);
}

void
stu_hash_remove_locked(stu_hash_t *hash, stu_uint_t key, u_char *name, size_t len) {
	stu_uint_t      i;
	stu_hash_elt_t *elts, *e;
	stu_queue_t    *q;

	i = key % hash->size;

	elts = hash->buckets[i];
	if (elts == NULL) {
		return;
	}

	for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, queue);
		if (e->key_hash != key || e->key.len != len) {
			continue;
		}
		if (stu_strncmp(e->key.data, name, len) == 0) {
			stu_queue_remove(&e->queue);
			stu_queue_remove(&e->q);

			if (hash->free) {
				hash->free(hash->pool, e->key.data);
				hash->free(hash->pool, e);
			}

			hash->length--;
			break;
		}
	}

	if (elts->queue.next == &elts->queue) {
		hash->buckets[i] = NULL;

		if (hash->free) {
			hash->free(hash->pool, elts);
		}
	}
}

void
stu_hash_foreach(stu_hash_t *hash, stu_hash_foreach_pt cb) {

}

void
stu_hash_free_empty_pt(void *pool, void *p) {

}

