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

static const stu_str_t  STU_MESSAGE_TYPE_UNI = stu_string("uni");
static const stu_str_t  STU_MESSAGE_TYPE_MULTI = stu_string("multi");

typedef struct stu_message_s stu_message_t;

struct stu_message_s {
	stu_int_t        id;
	stu_buf_t        data;
	stu_str_t       *type;
	time_t           time;

	stu_message_t   *next;
};

typedef struct {
	stu_base_pool_t *pool;

	stu_int_t        id;
	stu_int_t        state;

	stu_hash_t       userlist;
	stu_message_t   *messages;
} stu_channel_t;

static const stu_str_t  STU_RAW_DATA_IDENTITY = stu_string("identity");
static const stu_str_t  STU_RAW_DATA_MESSAGE = stu_string("message");
static const stu_str_t  STU_RAW_DATA_JOIN = stu_string("join");
static const stu_str_t  STU_RAW_DATA_LEFT = stu_string("left");
static const stu_str_t  STU_RAW_DATA_ERROR = stu_string("error");

static const stu_str_t  STU_ERR_EXP_BAD_REQUEST = stu_string("Bad Request");
static const stu_str_t  STU_ERR_EXP_UNAUTHORIZED = stu_string("Unauthorized");
static const stu_str_t  STU_ERR_EXP_FORBIDDEN = stu_string("Forbidden");
static const stu_str_t  STU_ERR_EXP_NOT_FOUND = stu_string("Not Found");
static const stu_str_t  STU_ERR_EXP_NOT_ACCEPTABLE = stu_string("Not Acceptable");
static const stu_str_t  STU_ERR_EXP_CONFLICT = stu_string("Conflict");

static const stu_str_t  STU_MESSAGE_TEMPLATE_RAW = stu_string("\"raw\":\"%s\"");
static const stu_str_t  STU_MESSAGE_TEMPLATE_DATA = stu_string("\"data\":{\"text\":\"%s\",\"type\":\"%s\"}");
static const stu_str_t  STU_MESSAGE_TEMPLATE_CHANNEL = stu_string("\"channel\":{\"id\":%ld,\"state\":%ld,\"role\":%d}");
static const stu_str_t  STU_MESSAGE_TEMPLATE_USER = stu_string("\"user\":{\"id\":%ld,\"name\":\"%s\"}");
static const stu_str_t  STU_MESSAGE_TEMPLATE_ERROR = stu_string("\"error\":{\"code\":%d,\"explain\":\"%s\"}");

#endif /* STU_CHANNEL_H_ */
