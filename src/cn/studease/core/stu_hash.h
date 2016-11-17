/*
 * stu_hash.h
 *
 *  Created on: 2016-9-14
 *      Author: Tony Lau
 */

#ifndef STU_HASH_H_
#define STU_HASH_H_

#include "stu_config.h"
#include "stu_core.h"

typedef struct stu_hash_elt_s stu_hash_elt_t;

struct stu_hash_elt_s {
	stu_queue_t       queue;

	stu_str_t         key;
	stu_uint_t        key_hash;
	void             *value;
};

typedef struct {
	stu_spinlock_t    lock;

	stu_slab_pool_t  *pool;

	stu_list_t        keys;    // type: stu_hash_elt_t *
	stu_hash_elt_t  **buckets;
	stu_uint_t        size;
	stu_uint_t        length;
} stu_hash_t;

typedef struct {
	stu_uint_t  hash;
	stu_str_t   key;
	stu_str_t   value;
	u_char     *lowcase_key;
} stu_table_elt_t;

typedef void (*stu_hash_pt) (stu_str_t *key, void *value);

#define stu_hash(key, c)        ((stu_uint_t) key * 31 + c)
#define STU_HASH_ELT_SIZE(name) \
	(sizeof(void *) + stu_align((name)->key.len + 2, sizeof(void *)))

stu_uint_t stu_hash_key(u_char *data, size_t len);

stu_int_t stu_hash_init(stu_hash_t *hash, stu_uint_t size, stu_slab_pool_t *pool);
stu_int_t stu_hash_insert(stu_hash_t *hash, stu_str_t *key, void *value);

void *stu_hash_find(stu_hash_t *hash, stu_str_t *key);
void  stu_hash_remove(stu_hash_t *hash, stu_str_t *key);

void stu_hash_foreach(stu_hash_t *hash, stu_hash_pt cb);

#endif /* STU_HASH_H_ */
