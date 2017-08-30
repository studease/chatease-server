/*
 * stu_protocol.c
 *
 *  Created on: Mar 5, 2017
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_str_t  STU_PROTOCOL_CMD = stu_string("cmd");
stu_str_t  STU_PROTOCOL_RAW = stu_string("raw");
stu_str_t  STU_PROTOCOL_REQ = stu_string("req");
stu_str_t  STU_PROTOCOL_DATA = stu_string("data");
stu_str_t  STU_PROTOCOL_TYPE = stu_string("type");
stu_str_t  STU_PROTOCOL_CHANNEL = stu_string("channel");
stu_str_t  STU_PROTOCOL_USER = stu_string("user");
stu_str_t  STU_PROTOCOL_ID = stu_string("id");
stu_str_t  STU_PROTOCOL_NAME = stu_string("name");
stu_str_t  STU_PROTOCOL_ROLE = stu_string("role");
stu_str_t  STU_PROTOCOL_STATE = stu_string("state");
stu_str_t  STU_PROTOCOL_STATUS = stu_string("status");
stu_str_t  STU_PROTOCOL_TOTAL = stu_string("total");
stu_str_t  STU_PROTOCOL_ERROR = stu_string("error");
stu_str_t  STU_PROTOCOL_CODE = stu_string("code");

stu_str_t  STU_PROTOCOL_CMDS_TEXT = stu_string("text");
stu_str_t  STU_PROTOCOL_CMDS_MUTE = stu_string("mute");
stu_str_t  STU_PROTOCOL_CMDS_KICKOUT = stu_string("kickout");
stu_str_t  STU_PROTOCOL_CMDS_EXTERN = stu_string("extern");
stu_str_t  STU_PROTOCOL_CMDS_PING = stu_string("ping");

stu_str_t  STU_PROTOCOL_RAWS_IDENT = stu_string("ident");
stu_str_t  STU_PROTOCOL_RAWS_TEXT = stu_string("text");
stu_str_t  STU_PROTOCOL_RAWS_JOIN = stu_string("join");
stu_str_t  STU_PROTOCOL_RAWS_LEFT = stu_string("left");
stu_str_t  STU_PROTOCOL_RAWS_USERS = stu_string("users");
stu_str_t  STU_PROTOCOL_RAWS_MUTE = stu_string("mute");
stu_str_t  STU_PROTOCOL_RAWS_KICKOUT = stu_string("kickout");
stu_str_t  STU_PROTOCOL_RAWS_ERROR = stu_string("error");
stu_str_t  STU_PROTOCOL_RAWS_PONG = stu_string("pong");
