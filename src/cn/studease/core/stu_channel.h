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

#define STU_MAX_CHANNEL_N      512
#define STU_USERLIST_PAGE_SIZE 1024

typedef struct {
	stu_atomic_t    lock;

	stu_hash_t      users; //type: stu_connection_t
} stu_user_page_t;

typedef struct {
	stu_queue_t     queue;

	stu_uint_t      id;
	stu_str_t       data;   // formated JSON string

	stu_int_t       unread;
} stu_message_t;

typedef struct {
	stu_uint_t      id;
	stu_int_t       status;

	stu_list_t      userlist; // type: stu_user_page_t
	stu_message_t  *messages;
} stu_channel_t;

#endif /* STU_CHANNEL_H_ */
