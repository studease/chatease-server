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

#if (__WORDSIZE == 64)

#define STU_MAX_CHANNEL_N  512
#define STU_MAX_USER_N     512

#else

#define STU_MAX_CHANNEL_N  1024
#define STU_MAX_USER_N     1024

#endif

#define STU_CHANNEL_ID_MAX_LEN 16

#define STU_CHANNEL_PUSH_USERS_DEFAULT_SIZE     1024
#define STU_CHANNEL_PUSH_USERS_DEFAULT_INTERVAL 30

typedef struct {
	stu_base_pool_t *pool; // not used currently.

	stu_str_t        id;
	stu_int_t        message_n;
	uint8_t          state;

	stu_hash_t       userlist;
} stu_channel_t;


void  stu_channel_push_online_users(stu_event_t *ev);
void  stu_channel_broadcast(stu_str_t *id, void *ch);

#endif /* STU_CHANNEL_H_ */
