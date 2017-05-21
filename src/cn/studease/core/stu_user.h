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
	uint8_t     code;
	stu_uint_t  time;
} stu_punishment_t;

typedef struct {
	stu_int_t         id;
	stu_str_t         name;

	stu_str_t         strid;
	u_char            idstr[9];

	uint8_t           role;
	stu_short_t       interval;
	stu_uint_t        active;
	stu_punishment_t  punishment;

	stu_channel_t    *channel;
} stu_user_t;

stu_int_t stu_user_init(stu_user_t *usr, stu_int_t id, stu_str_t *name, stu_base_pool_t *pool);

#endif /* STU_USER_H_ */
