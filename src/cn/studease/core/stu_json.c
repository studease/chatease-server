/*
 * stu_json.c
 *
 *  Created on: 2017-3-3
 *      Author: Tony Lau
 */

#include <float.h>
#include "stu_config.h"
#include "stu_core.h"

static void *(*stu_json_malloc)(size_t size) = malloc;
static void (*stu_json_free)(void *ptr) = free;

static const stu_str_t  STU_JSON_VALUE_NULL = stu_string("null");
static const stu_str_t  STU_JSON_VALUE_TRUE = stu_string("true");
static const stu_str_t  STU_JSON_VALUE_FALSE = stu_string("false");

static stu_int_t  stu_json_set_key(stu_json_t *item, stu_str_t *key);

static size_t  stu_json_parse_value(stu_json_t *item, u_char *data, size_t len, u_char **err);
static size_t  stu_json_parse_specific(stu_json_t *item, u_char *data, size_t len, u_char **err);
static size_t  stu_json_parse_string(stu_json_t *item, u_char *data, size_t len, u_char **err);
static size_t  stu_json_parse_number(stu_json_t *item, u_char *data, size_t len, u_char **err);
static size_t  stu_json_parse_array(stu_json_t *item, u_char *data, size_t len, u_char **err);
static size_t  stu_json_parse_object(stu_json_t *item, u_char *data, size_t len, u_char **err);

static u_char *stu_json_print_value(stu_json_t *item, u_char *dst);
static u_char *stu_json_print_null(stu_json_t *item, u_char *dst);
static u_char *stu_json_print_true(stu_json_t *item, u_char *dst);
static u_char *stu_json_print_false(stu_json_t *item, u_char *dst);
static u_char *stu_json_print_string(stu_json_t *item, u_char *dst);
static u_char *stu_json_print_number(stu_json_t *item, u_char *dst);
static u_char *stu_json_print_array(stu_json_t *array, u_char *dst);
static u_char *stu_json_print_object(stu_json_t *object, u_char *dst);

static u_char *stu_json_print_key(stu_json_t *item, u_char *dst);


void
stu_json_init_hooks(stu_json_hooks_t *hooks) {
	if (hooks == NULL) {
		stu_json_malloc = malloc;
		stu_json_free = free;

		return;
	}

	stu_json_malloc = hooks->malloc_fn ? hooks->malloc_fn : malloc;
	stu_json_free = hooks->free_fn ? hooks->free_fn : free;
}

stu_json_t *
stu_json_create(u_char type, stu_str_t *key) {
	stu_json_t *item;
	size_t      size;

	size = sizeof(stu_json_t);
	item = (stu_json_t *) stu_json_malloc(size);
	if (item == NULL) {
		return NULL;
	}

	stu_memzero(item, size);
	item->prev = item;
	item->type = type;

	if (stu_json_set_key(item, key) != STU_OK) {
		stu_json_delete(item);
		return NULL;
	}

	return item;
}

stu_json_t *
stu_json_create_null(stu_str_t *key) {
	stu_json_t *item;

	item = stu_json_create(STU_JSON_TYPE_NULL, key);
	if (item == NULL) {
		return NULL;
	}

	return item;
}

stu_json_t *
stu_json_create_bool(stu_str_t *key, stu_bool_t bool) {
	stu_json_t *item;

	item = stu_json_create(bool ? STU_JSON_TYPE_TRUE : STU_JSON_TYPE_FALSE, key);
	if (item == NULL) {
		return NULL;
	}

	return item;
}

stu_json_t *
stu_json_create_true(stu_str_t *key) {
	return stu_json_create_bool(key, TRUE);
}

stu_json_t *
stu_json_create_false(stu_str_t *key) {
	return stu_json_create_bool(key, FALSE);
}

stu_json_t *
stu_json_create_string(stu_str_t *key, u_char *value, size_t len) {
	stu_json_t *item;
	size_t      size;
	stu_str_t  *str;

	item = stu_json_create(STU_JSON_TYPE_STRING, key);
	if (item == NULL) {
		return NULL;
	}

	size = sizeof(stu_str_t);
	item->value = stu_json_malloc(size);
	if (item->value == NULL) {
		stu_json_delete(item);
		return NULL;
	}

	str = (stu_str_t *) item->value;
	str->data = stu_json_malloc(len + 1);
	if (str->data == NULL) {
		stu_json_delete(item);
		return NULL;
	}

	stu_strncpy(str->data, value, len);
	str->len = len;

	return item;
}

stu_json_t *
stu_json_create_number(stu_str_t *key, stu_double_t num) {
	stu_json_t *item;

	item = stu_json_create(STU_JSON_TYPE_NUMBER, key);
	if (item == NULL) {
		return NULL;
	}

	item->value = stu_json_malloc(8);
	if (item->value == NULL) {
		stu_json_delete(item);
		return NULL;
	}

	*(stu_double_t *) item->value = num;

	return item;
}

stu_json_t *
stu_json_create_array(stu_str_t *key) {
	stu_json_t *item;

	item = stu_json_create(STU_JSON_TYPE_ARRAY, key);
	if (item == NULL) {
		return NULL;
	}

	return item;
}

stu_json_t *
stu_json_create_object(stu_str_t *key) {
	stu_json_t *item;

	item = stu_json_create(STU_JSON_TYPE_OBJECT, key);
	if (item == NULL) {
		return NULL;
	}

	return item;
}

stu_json_t *
stu_json_duplicate(stu_json_t *item, stu_bool_t recurse) {
	stu_json_t   *copy, *child, *newchild;
	stu_str_t    *str;
	stu_double_t *num;

	if (item == NULL) {
		return NULL;
	}

	switch (item->type) {
	case STU_JSON_TYPE_STRING:
		str = (stu_str_t *) item->value;
		copy = stu_json_create_string(&item->key, str->data, str->len);
		break;

	case STU_JSON_TYPE_NUMBER:
		num = (stu_double_t *) item->value;
		copy = stu_json_create_number(&item->key, *num);
		break;

	case STU_JSON_TYPE_ARRAY:
	case STU_JSON_TYPE_OBJECT:
		copy = stu_json_create(item->type, &item->key);
		if (recurse == TRUE && copy != NULL) {
			for (child = (stu_json_t *) item->value; child; child = child->next) {
				newchild = stu_json_duplicate(child, recurse);
				if (newchild == NULL) {
					goto failed;
				}

				stu_json_add_item_to_object(copy, newchild);
			}
		}
		break;

	default:
		copy = stu_json_create(item->type, &item->key);
		break;
	}

	return copy;

failed:

	stu_json_delete(copy);

	return NULL;
}

static stu_int_t
stu_json_set_key(stu_json_t *item, stu_str_t *key) {
	if (key != NULL) {
		if (item->key.len < key->len) {
			if (item->key.data != NULL) {
				stu_json_free(item->key.data);
			}

			item->key.data = (u_char *) stu_json_malloc(key->len + 1);
			if (item->key.data == NULL) {
				return STU_ERROR;
			}
		}

		stu_strncpy(item->key.data, key->data, key->len);
		item->key.len = key->len;
	}

	return STU_OK;
}


void
stu_json_add_item_to_array(stu_json_t *array, stu_json_t *item) {
	stu_json_add_item_to_object(array, item);
}

void
stu_json_add_item_to_object(stu_json_t *object, stu_json_t *item) {
	stu_json_t *child;

	if (object == NULL || item == NULL) {
		return;
	}

	if (object->value == NULL) {
		object->value = item;
		item->prev = item; // already set when creating
		return;
	}

	child = (stu_json_t *) object->value;
	child->prev->next = item;
	item->prev = child->prev;
	child->prev = item;
}


stu_json_t *
stu_json_get_array_item_at(stu_json_t *array, stu_int_t index) {
	stu_json_t *item;

	if (array == NULL) {
		return NULL;
	}

	for (item = (stu_json_t *) array->value; item && index; item = item->next, index--) {
		/* void */
	}

	return item;
}

stu_json_t *
stu_json_get_object_item_by(stu_json_t *object, stu_str_t *key) {
	stu_json_t *item;

	if (object == NULL) {
		return NULL;
	}

	for (item = (stu_json_t *) object->value; item; item = item->next) {
		if (item->key.len != key->len) {
			continue;
		}

		if (stu_strncmp(item->key.data, key->data, key->len) == 0) {
			break;
		}
	}

	return item;
}


stu_json_t *
stu_json_remove_item_from_array(stu_json_t *array, stu_int_t index) {
	stu_json_t *item;

	item = stu_json_get_array_item_at(array, index);
	if (item != NULL) {
		item->prev->next = item->next;
		if (item->next != NULL) {
			item->next->prev = item->prev;
		}

		item->prev = item;
		item->next = NULL;
	}

	return item;
}

stu_json_t *
stu_json_remove_item_from_object(stu_json_t *object, stu_str_t *key) {
	stu_json_t *item;

	item = stu_json_get_object_item_by(object, key);
	if (item != NULL) {
		item->prev->next = item->next;
		if (item->next != NULL) {
			item->next->prev = item->prev;
		}

		item->prev = item;
		item->next = NULL;
	}

	return item;
}


void
stu_json_delete(stu_json_t *item) {
	stu_json_t *child;
	stu_str_t  *str;

	if (item == NULL) {
		return;
	}

	switch (item->type) {
	case STU_JSON_TYPE_STRING:
		str = (stu_str_t *) item->value;
		if (str != NULL && str->data != NULL) {
			stu_json_free(str->data);
		}
	case STU_JSON_TYPE_NUMBER:
		if (item->value != NULL) {
			stu_json_free(item->value);
		}
		break;
	case STU_JSON_TYPE_ARRAY:
	case STU_JSON_TYPE_OBJECT:
		for (child = (stu_json_t *) item->value; child; child = child->next) {
			stu_json_delete(child);
		}
		break;
	default:
		break;
	}

	if (item->key.data != NULL) {
		stu_json_free(item->key.data);
	}

	stu_json_free(item);
}

void
stu_json_delete_item_from_array(stu_json_t *array, stu_int_t index) {
	stu_json_t *item;

	item = stu_json_remove_item_from_array(array, index);
	stu_json_delete(item);
}

void
stu_json_delete_item_from_object(stu_json_t *object, stu_str_t *key) {
	stu_json_t *item;

	item = stu_json_remove_item_from_object(object, key);
	stu_json_delete(item);
}


stu_json_t *
stu_json_parse(u_char *data, size_t len) {
	stu_json_t *item;
	u_char     *err;

	err = NULL;

	item = stu_json_create(STU_JSON_TYPE_NONE, NULL);
	if (item == NULL) {
		return NULL;
	}

	stu_json_parse_value(item, data, len, &err);
	if (err != NULL) {
		stu_log_error(0, "Failed to parse JSON: %s", err);

		stu_json_delete(item);

		return NULL;
	}

	return item;
}

static size_t
stu_json_parse_value(stu_json_t *item, u_char *data, size_t len, u_char **err) {
	size_t  pos, n;
	u_char *p, c;

	pos = 0;
	p = data;

	// skip blank spaces
	for (c = *p; c == ' '; p++, pos++) {
		/* void */
	}

	switch (c) {
	case '\"':
		item->type = STU_JSON_TYPE_STRING;

		n = stu_json_parse_string(item, p, len - pos, err);
		pos += n;
		break;
	case '{':
		item->type = STU_JSON_TYPE_OBJECT;

		n = stu_json_parse_object(item, p, len - pos, err);
		pos += n;
		break;
	case '[':
		item->type = STU_JSON_TYPE_ARRAY;

		n = stu_json_parse_array(item, p, len - pos, err);
		pos += n;
		break;
	case ' ':
		// skip
		break;
	default:
		if (c == '-' || (c >= '0' && c <= '9')) {
			item->type = STU_JSON_TYPE_NUMBER;

			n = stu_json_parse_number(item, p, len - pos, err);
			pos += n;
		} else {
			n = stu_json_parse_specific(item, p, len - pos, err);
			pos += n;
		}
		break;
	}

	return pos;
}

static size_t
stu_json_parse_specific(stu_json_t *item, u_char *data, size_t len, u_char **err) {
	size_t  pos, n;
	u_char *p, c;

	pos = 0;
	p = data;

	for (c = *p; c == ' '; p++, pos++) {
		/* void */
	}

	if (stu_strncmp(p, STU_JSON_VALUE_NULL.data, STU_JSON_VALUE_NULL.len) == 0) {
		item->type = STU_JSON_TYPE_NULL;
		n = 4;
	} else if (stu_strncmp(p, STU_JSON_VALUE_TRUE.data, STU_JSON_VALUE_TRUE.len) == 0) {
		item->type = STU_JSON_TYPE_TRUE;
		n = 4;
	} else if (stu_strncmp(p, STU_JSON_VALUE_FALSE.data, STU_JSON_VALUE_FALSE.len) == 0) {
		item->type = STU_JSON_TYPE_FALSE;
		n = 5;
	} else {
		goto failed;
	}

	pos += n;

	return pos;

failed:

	*err = p;

	return pos;
}

static size_t
stu_json_parse_string(stu_json_t *item, u_char *data, size_t len, u_char **err) {
	size_t     pos;
	u_char    *p, *s, c;
	stu_str_t *str;
	enum {
		sw_start = 0,
		sw_str_start,
		sw_str,
		sw_str_end
	} state;

	state = sw_start;

	for (pos = 0, p = data; pos < len; pos++, p++) {
		c = *p;

		switch (state) {
		case sw_start:
			if (c == '\"') {
				state = sw_str_start;
			} else if (c == ' ') {
				// skip
			} else {
				goto failed;
			}
			break;

		case sw_str_start:
			item->value = stu_json_malloc(sizeof(stu_str_t));
			if (item->value == NULL) {
				goto failed;
			}

			str = (stu_str_t *) item->value;
			str->data = s = p;

			if (c == '\"') {
				str->data = NULL;
				str->len = 0;
				state = sw_str_end;
			} else {
				state = sw_str;
			}
			break;

		case sw_str:
			if (c == '\"') {
				str = (stu_str_t *) item->value;
				str->len = p - s;

				str->data = stu_json_malloc(str->len + 1);
				if (str->data == NULL) {
					goto failed;
				}

				stu_strncpy(str->data, s, str->len);

				state = sw_str_end;
			} else {
				// appending
			}
			break;

		case sw_str_end:
			goto done;
			break;

		default:
			break;
		}
	}

done:

	return pos;

failed:

	*err = p;

	return pos;
}

static size_t
stu_json_parse_number(stu_json_t *item, u_char *data, size_t len, u_char **err) {
	size_t        pos;
	u_char       *p, c, *endptr;
	stu_double_t  num;

	pos = 0;
	p = data;
	endptr = NULL;

	for (c = *p; c == ' '; p++, pos++) {
		/* void */
	}

	num = strtod((const char*) data, (char**) &endptr);
	if (data == endptr) {
		goto failed;
	}

	item->value = stu_json_malloc(8);
	if (item->value == NULL) {
		goto failed;
	}

	*(stu_double_t *) item->value = num;

	pos += endptr - data;

	return pos;

failed:

	*err = p;

	return pos;
}

static size_t
stu_json_parse_array(stu_json_t *array, u_char *data, size_t len, u_char **err) {
	size_t      pos, n;
	u_char     *p, c;
	stu_json_t *item;
	enum {
		sw_start = 0,
		sw_arr_start,
		sw_arr_end
	} state;

	state = sw_start;

	for (pos = 0, p = data; pos < len; pos++, p++) {
		c = *p;

		switch (state) {
		case sw_start:
			if (c == '[') {
				state = sw_arr_start;
			} else if (c == ' ') {
				// skip
			} else {
				goto failed;
			}
			break;

		case sw_arr_start:
			item = stu_json_create(STU_JSON_TYPE_NONE, NULL);
			if (item == NULL) {
				goto failed;
			}

			n = stu_json_parse_value(item, p, len - pos, err);
			pos += n;

			if (*err != NULL) {
				stu_json_delete(item);
				return pos;
			}

			stu_json_add_item_to_array(array, item);

			for (p += n; pos < len; pos++, p++) {
				c = *p;

				if (c == ' ') {
					continue;
				}

				if (c == ',') {
					state = sw_arr_start;
					break;
				} else if (c == ']') {
					state = sw_arr_end;
					break;
				} else {
					goto failed;
				}
			}
			break;

		case sw_arr_end:
			goto done;
			break;

		default:
			break;
		}
	}

done:

	return pos;

failed:

	*err = p;

	return pos;
}

static size_t
stu_json_parse_object(stu_json_t *object, u_char *data, size_t len, u_char **err) {
	size_t      pos, n;
	u_char     *p, *s, c;
	stu_json_t *item;
	enum {
		sw_start = 0,
		sw_obj_start,
		sw_key_start,
		sw_key,
		sw_key_end,
		sw_val,
		sw_obj_end
	} state;

	state = sw_start;

	for (pos = 0, p = data; pos < len; pos++, p++) {
		c = *p;

		switch (state) {
		case sw_start:
			if (c == '{') {
				state = sw_obj_start;
			} else if (c == ' ') {
				// skip
			} else {
				goto failed;
			}
			break;

		case sw_obj_start:
			if (c == '\"') {
				state = sw_key_start;
			} else if (c == ' ') {
				// skip
			} else {
				goto failed;
			}
			break;

		case sw_key_start:
			item = stu_json_create(STU_JSON_TYPE_NONE, NULL);
			if (item == NULL) {
				goto failed;
			}

			item->key.data = s = p;

			if (c == '\"') {
				item->key.data = NULL;
				item->key.len = 0;
				state = sw_key_end;
			} else {
				state = sw_key;
			}
			break;

		case sw_key:
			if (c == '\"') {
				item->key.len = p - s;

				item->key.data = stu_json_malloc(item->key.len + 1);
				if (item->key.data == NULL) {
					goto failed;
				}

				stu_strncpy(item->key.data, s, item->key.len);

				state = sw_key_end;
			} else {
				// appending
			}
			break;

		case sw_key_end:
			if (c == ':') {
				state = sw_val;
			} else if (c == ' ') {
				// skip
			} else {
				goto failed;
			}
			break;

		case sw_val:
			n = stu_json_parse_value(item, p, len - pos, err);
			pos += n;

			if (*err != NULL) {
				stu_json_delete(item);
				return pos;
			}

			stu_json_add_item_to_object(object, item);

			for (p += n; pos < len; pos++, p++) {
				c = *p;

				if (c == ' ') {
					continue;
				}

				if (c == ',') {
					state = sw_obj_start;
					break;
				} else if (c == '}') {
					state = sw_obj_end;
					break;
				} else {
					goto failed;
				}
			}
			break;

		case sw_obj_end:
			goto done;
			break;

		default:
			break;
		}
	}

done:

	return pos;

failed:

	*err = p;

	return pos;
}


u_char *
stu_json_stringify(stu_json_t *item, u_char *dst) {
	return stu_json_print_value(item, dst);
}

static u_char *
stu_json_print_value(stu_json_t *item, u_char *dst) {
	u_char *p;

	switch (item->type) {
	case STU_JSON_TYPE_NULL:
		p = stu_json_print_null(item, dst);
		break;

	case STU_JSON_TYPE_TRUE:
		p = stu_json_print_true(item, dst);
		break;

	case STU_JSON_TYPE_FALSE:
		p = stu_json_print_false(item, dst);
		break;

	case STU_JSON_TYPE_STRING:
		p = stu_json_print_string(item, dst);
		break;

	case STU_JSON_TYPE_NUMBER:
		p = stu_json_print_number(item, dst);
		break;

	case STU_JSON_TYPE_ARRAY:
		p = stu_json_print_array(item, dst);
		break;

	case STU_JSON_TYPE_OBJECT:
		p = stu_json_print_object(item, dst);
		break;

	default:
		break;
	}

	return p;
}

static u_char *
stu_json_print_null(stu_json_t *item, u_char *dst) {
	return stu_strncpy(dst, STU_JSON_VALUE_NULL.data, STU_JSON_VALUE_NULL.len);
}

static u_char *
stu_json_print_true(stu_json_t *item, u_char *dst) {
	return stu_strncpy(dst, STU_JSON_VALUE_TRUE.data, STU_JSON_VALUE_TRUE.len);
}

static u_char *
stu_json_print_false(stu_json_t *item, u_char *dst) {
	return stu_strncpy(dst, STU_JSON_VALUE_FALSE.data, STU_JSON_VALUE_FALSE.len);
}

static u_char *
stu_json_print_string(stu_json_t *item, u_char *dst) {
	stu_str_t *str;

	str = (stu_str_t *) item->value;

	*dst++ = '\"';
	dst = stu_strncpy(dst, str->data, str->len);
	*dst++ = '\"';

	return dst;
}

static u_char *
stu_json_print_number(stu_json_t *item, u_char *dst) {
	stu_double_t  d;
	stu_int_t     i;

	d = *(stu_double_t *) item->value;
	i = (stu_int_t) d;

	if (d == 0) {
		*dst++ = '0';
	} else if ((fabs(d - (stu_double_t) i) <= DBL_EPSILON) && (d <= INT_MAX) && (d >= INT_MIN)) {
		stu_sprintf(dst, "%d", i);
	} else {
		if ((d * 0) != 0) { // NaN or Infinity
			dst = stu_strncpy(dst, STU_JSON_VALUE_NULL.data, STU_JSON_VALUE_NULL.len);
		} else if ((fabs(floor(d) - d) <= DBL_EPSILON) && (fabs(d) < 1.0e60)) {
			stu_sprintf(dst, "%.0f", d);
		} else if ((fabs(d) < 1.0e-6) || (fabs(d) > 1.0e9)) {
			stu_sprintf(dst, "%e", d);
		} else {
			stu_sprintf(dst, "%f", d);
		}
	}

	return dst;
}

static u_char *
stu_json_print_array(stu_json_t *array, u_char *dst) {
	stu_json_t *item;

	*dst++ = '[';

	for (item = (stu_json_t *) array->value; item; item = item->next) {
		dst = stu_json_print_value(item, dst);

		if (item->next != NULL) {
			*dst++ = ',';
		}
	}

	*dst++ = ']';

	return dst;
}

static u_char *
stu_json_print_object(stu_json_t *object, u_char *dst) {
	stu_json_t *item;

	*dst++ = '{';

	for (item = (stu_json_t *) object->value; item; item = item->next) {
		dst = stu_json_print_key(item, dst);

		*dst++ = ':';

		dst = stu_json_print_value(item, dst);

		if (item->next != NULL) {
			*dst++ = ',';
		}
	}

	*dst++ = '}';

	return dst;
}

static u_char *
stu_json_print_key(stu_json_t *item, u_char *dst) {
	*dst++ = '\"';
	dst = stu_strncpy(dst, item->key.data, item->key.len);
	*dst++ = '\"';

	return dst;
}

