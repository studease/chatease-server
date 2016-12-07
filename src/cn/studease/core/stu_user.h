/*
 * stu_user.h
 *
 *  Created on: 2016-9-28
 *      Author: Tony Lau
 */

#ifndef STU_USER_H_
#define STU_USER_H_

#include "stu_config.h"
#include "stu_core.h"

typedef struct {
	stu_int_t            id;
	stu_str_t            name;

	stu_str_t            strid;
	u_char               idstr[8];

	stu_channel_t       *channel;
} stu_user_t;

stu_int_t stu_user_init(stu_user_t *usr, stu_int_t id, stu_str_t *name, stu_base_pool_t *pool);

#endif /* STU_USER_H_ */
