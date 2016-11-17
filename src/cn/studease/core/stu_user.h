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
	stu_queue_t          queue;

	stu_uint_t           id;       // channel ID
	stu_int_t            role;
	stu_uint_t           rights;

	stu_message_t       *lastsent;
} stu_user_property_t;

typedef struct {
	stu_int_t            id;
	stu_str_t            name;
	stu_user_property_t *properties; // properties in channels
	time_t               time;       // last posting time
} stu_user_t;


stu_int_t stu_user_init(stu_user_t *user);

#endif /* STU_USER_H_ */
