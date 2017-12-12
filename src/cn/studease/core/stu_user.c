/*
 * stu_user.c
 *
 *  Created on: 2016-11-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static uint16_t stu_user_get_interval(uint8_t role);


stu_int_t
stu_user_init(stu_user_t *usr, stu_str_t *id, stu_str_t *name) {
	if (id && id->len) {
		if (usr->id.data == NULL || usr->id.len < id->len) {
			usr->id.data = stu_calloc(id->len + 1);
			if (usr->id.data == NULL) {
				stu_log_error(0, "Failed to alloc memory for user id.");
				return STU_ERROR;
			}
		}

		memcpy(usr->id.data, id->data, id->len);
	}

	if (name && name->len) {
		if (usr->name.data == NULL || usr->name.len < name->len) {
			usr->name.data = stu_calloc(name->len + 1);
			if (usr->name.data == NULL) {
				stu_log_error(0, "Failed to alloc memory for user name.");
				return STU_ERROR;
			}
		}

		memcpy(usr->name.data, name->data, name->len);
	}

	stu_str_null(&usr->icon);
	usr->role = STU_USER_ROLE_VISITOR;
	usr->interval = stu_user_get_interval(usr->role);
	usr->active = 0;

	usr->punishment = NULL;
	usr->channel = NULL;

	return STU_OK;
}

void
stu_user_set_role(stu_user_t *usr, uint8_t role) {
	usr->role = role;
	usr->interval = stu_user_get_interval(role);
}


static uint16_t
stu_user_get_interval(uint8_t role) {
	uint16_t  interval, vip;

	interval = 2000;

	if (role & 0xF0) {
		interval = 0;
	} else if (role) {
		interval *= .5;
		vip = role - 1;
		if (vip) {
			interval = interval >= vip * 100 ? interval - vip * 100 : 0;
		}
	}

	return interval;
}

