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

typedef struct stu_userinfo_s stu_userinfo_t;

typedef struct {
	u_char               id;
	stu_str_t           *explain;
} stu_error_t;

struct stu_userinfo_s {
	stu_int_t            id; // channel ID
	u_char               role;
	u_char               rights;

	stu_message_t       *last;
	time_t               time;

	stu_userinfo_t      *next;
};

typedef struct {
	stu_int_t            id;
	stu_str_t            name;

	stu_userinfo_t      *info;
} stu_user_t;


stu_int_t stu_user_init(stu_user_t *user);

#endif /* STU_USER_H_ */
