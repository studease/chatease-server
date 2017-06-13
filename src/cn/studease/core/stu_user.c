/*
 * stu_user.c
 *
 *  Created on: 2016-11-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_int_t
stu_user_init(stu_user_t *usr, stu_str_t *id, stu_str_t *name, stu_base_pool_t *pool) {
	if (id && id->len) {
		if (usr->id.data == NULL || usr->id.len < id->len) {
			usr->id.data = stu_base_pcalloc(pool, id->len + 1);
			if (usr->id.data == NULL) {
				stu_log_error(0, "Failed to alloc memory for user id.");
				return STU_ERROR;
			}
		}

		memcpy(usr->id.data, id->data, id->len);
	}

	if (name && name->len) {
		if (usr->name.data == NULL || usr->name.len < name->len) {
			usr->name.data = stu_base_pcalloc(pool, name->len + 1);
			if (usr->name.data == NULL) {
				stu_log_error(0, "Failed to alloc memory for user name.");
				return STU_ERROR;
			}
		}

		memcpy(usr->name.data, name->data, name->len);
	}

	return STU_OK;
}

