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

	stu_int_t            channel_id;
	stu_channel_t       *channel;
} stu_user_t;

stu_int_t stu_user_init(stu_user_t *user);

#endif /* STU_USER_H_ */
