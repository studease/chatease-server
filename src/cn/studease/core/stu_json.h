/*
 * stu_json.h
 *
 *  Created on: 2017-2-27
 *      Author: Tony Lau
 */

#ifndef STU_JSON_H_
#define STU_JSON_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_JSON_TYPE_NONE    0x00
#define STU_JSON_TYPE_NULL    0x01
#define STU_JSON_TYPE_BOOLEAN 0x02
#define STU_JSON_TYPE_STRING  0x04
#define STU_JSON_TYPE_NUMBER  0x08
#define STU_JSON_TYPE_ARRAY   0x10
#define STU_JSON_TYPE_OBJECT  0x20

typedef struct stu_json_s stu_json_t;

struct stu_json_s {
	uint8_t     type;
	stu_str_t   key;
	uintptr_t   value;

	stu_json_t *prev;
	stu_json_t *next;
};

typedef struct {
	void *(*malloc_fn)(size_t size);
	void  (*free_fn)(void *ptr);
} stu_json_hooks_t;


void  stu_json_init_hooks(stu_json_hooks_t *hooks);

stu_json_t *stu_json_create(u_char type, stu_str_t *key);
stu_json_t *stu_json_create_null(stu_str_t *key);
stu_json_t *stu_json_create_bool(stu_str_t *key, stu_bool_t bool);
stu_json_t *stu_json_create_true(stu_str_t *key);
stu_json_t *stu_json_create_false(stu_str_t *key);
stu_json_t *stu_json_create_string(stu_str_t *key, u_char *value, size_t len);
stu_json_t *stu_json_create_number(stu_str_t *key, stu_double_t num);
stu_json_t *stu_json_create_array(stu_str_t *key);
stu_json_t *stu_json_create_object(stu_str_t *key);

stu_json_t *stu_json_duplicate(stu_json_t *item, stu_bool_t recurse);

void  stu_json_add_item_to_array(stu_json_t *array, stu_json_t *item);
void  stu_json_add_item_to_object(stu_json_t *object, stu_json_t *item);

stu_json_t *stu_json_get_array_item_at(stu_json_t *array, stu_int_t index);
stu_json_t *stu_json_get_object_item_by(stu_json_t *object, stu_str_t *key);

stu_json_t *stu_json_remove_item_from_array(stu_json_t *array, stu_int_t index);
stu_json_t *stu_json_remove_item_from_object(stu_json_t *object, stu_str_t *key);

void  stu_json_delete(stu_json_t *item);

void  stu_json_delete_item_from_array(stu_json_t *array, stu_int_t index);
void  stu_json_delete_item_from_object(stu_json_t *object, stu_str_t *key);

stu_json_t *stu_json_parse(u_char *data, size_t len);
u_char *stu_json_stringify(stu_json_t *item, u_char *dst);

#endif /* STU_JSON_H_ */
