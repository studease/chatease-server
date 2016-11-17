/*
 * stu_user.c
 *
 *  Created on: 2016-11-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_int_t
stu_user_init(stu_user_t *user) {
	stu_memzero(user, sizeof(stu_user_t));

	if (user->properties) {
		stu_queue_init(&user->properties->queue);
	}

	return STU_OK;
}

