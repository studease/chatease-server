/*
 * stu_user.c
 *
 *  Created on: 2016-11-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_int_t
stu_user_init(stu_user_t *u) {
	stu_memzero(u, sizeof(stu_user_t));

	return STU_OK;
}

