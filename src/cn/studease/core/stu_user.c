/*
 * stu_user.c
 *
 *  Created on: 2016-11-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_int_t
stu_user_init(stu_user_t *usr, stu_int_t id, stu_str_t *name, stu_base_pool_t *pool) {
	u_char *p;

	usr->id = id;

	p = usr->idstr;
	stu_sprintf(p, "%ld", id);

	usr->strid.data = usr->idstr;
	usr->strid.len = stu_strlen(usr->idstr);

	if (name) {
		if (usr->name.data == NULL || usr->name.len < name->len) {
			usr->name.data = stu_base_pcalloc(pool, name->len);
			if (usr->name.data == NULL) {
				stu_log_error(0, "Failed to alloc memory for user name.");
				return STU_ERROR;
			}
		}

		memcpy(usr->name.data, name->data, name->len);
	}

	return STU_OK;
}

