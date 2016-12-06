/*
 * stu_channel.h
 *
 *  Created on: 2016-9-28
 *      Author: Tony Lau
 */

#ifndef STU_CHANNEL_H_
#define STU_CHANNEL_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_MAX_CHANNEL_N                  512
#define STU_CHANNEL_USERPAGE_DEFAULT_SIZE  1024

typedef struct {
	stu_base_pool_t *pool;

	stu_int_t        id;
	stu_int_t        message_n;
	stu_int_t        state;

	stu_hash_t       userlist;
} stu_channel_t;

#endif /* STU_CHANNEL_H_ */
