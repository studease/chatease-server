/*
 * stu_websocket_request.h
 *
 *  Created on: 2016-11-24
 *      Author: Tony Lau
 */

#ifndef STU_WEBSOCKET_REQUEST_H_
#define STU_WEBSOCKET_REQUEST_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_WEBSOCKET_REQUEST_DEFAULT_SIZE  1024

#define STU_WEBSOCKET_OPCODE_TEXT           0x1
#define STU_WEBSOCKET_OPCODE_CLOSE          0x8
#define STU_WEBSOCKET_OPCODE_PING           0x9
#define STU_WEBSOCKET_OPCODE_PONG           0xA

#define STU_WEBSOCKET_TARGET_TYPE_MASK      0x80000000
#define STU_WEBSOCKET_TARGET_ID_MASK        0x7FFFFFFF

#define STU_WEBSOCKET_CMD_JOIN      0x01
#define STU_WEBSOCKET_CMD_LEFT      0x02
#define STU_WEBSOCKET_CMD_UNI_DATA  0x04
#define STU_WEBSOCKET_CMD_DATA      0X08

#define STU_WEBSOCKET_CMD_MUTE      0x11
#define STU_WEBSOCKET_CMD_UNMUTE    0x12
#define STU_WEBSOCKET_CMD_FORBID    0x14
#define STU_WEBSOCKET_CMD_RELIVE    0x18

#define STU_WEBSOCKET_CMD_UPGRADE   0x21
#define STU_WEBSOCKET_CMD_DEMOTE    0x22

#define STU_WEBSOCKET_CMD_DISMISS   0x41
#define STU_WEBSOCKET_CMD_ACTIVE    0x42

#define STU_WEBSOCKET_RAW_UNKNOWN   0x00
#define STU_WEBSOCKET_RAW_IDENTITY  0x01
#define STU_WEBSOCKET_RAW_MESSAGE   0x02
#define STU_WEBSOCKET_RAW_JOIN      0x04
#define STU_WEBSOCKET_RAW_LEFT      0x08
#define STU_WEBSOCKET_RAW_ERROR     0xFF

typedef struct stu_websocket_frame_s stu_websocket_frame_t;

struct stu_websocket_frame_s {
	u_char                 fin:1;
	u_char                 rsv1:1;
	u_char                 rsv2:1;
	u_char                 rsv3:1;
	u_char                 opcode:4;
	u_char                 mask:1;
	u_char                 payload_len:7;
	uint64_t               extended;
	u_char                 masking_key[4];
	stu_buf_t              payload_data;

	stu_websocket_frame_t *next;
};

typedef struct {
	stu_connection_t      *connection;

	stu_buf_t             *frame_in;

	stu_websocket_frame_t  frames_in;
	stu_websocket_frame_t  frames_out;

	u_char                 command;
	stu_int_t              status;

	// used for parsing request.
	stu_uint_t             state;
	stu_websocket_frame_t *frame;
} stu_websocket_request_t;

void stu_websocket_wait_request_handler(stu_event_t *rev);
void stu_websocket_request_handler(stu_event_t *wev);

stu_websocket_request_t *stu_websocket_create_request(stu_connection_t *c);
void stu_websocket_process_request(stu_websocket_request_t *r);

void stu_websocket_finalize_request(stu_websocket_request_t *r, u_char raw, stu_message_t *msg, stu_channel_t *ch, stu_userinfo_t *info, stu_error_t *err);

void stu_websocket_close_request(stu_websocket_request_t *r, stu_int_t rc);
void stu_websocket_free_request(stu_websocket_request_t *r, stu_int_t rc);
void stu_websocket_close_connection(stu_connection_t *c);

void stu_websocket_empty_handler(stu_event_t *ev);
void stu_websocket_request_empty_handler(stu_websocket_request_t *r);

#endif /* STU_WEBSOCKET_REQUEST_H_ */
