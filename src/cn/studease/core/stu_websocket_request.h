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

#define STU_WEBSOCKET_REQUEST_DEFAULT_SIZE  2048

#define STU_WEBSOCKET_CMD_JOIN      0x01
#define STU_WEBSOCKET_CMD_LEFT      0x02
#define STU_WEBSOCKET_CMD_UNI_DATA  0x04
#define STU_WEBSOCKET_CMD_DATA      0X08

#define STU_WEBSOCKET_CMD_MUTE      0X11
#define STU_WEBSOCKET_CMD_UNMUTE    0X12
#define STU_WEBSOCKET_CMD_FORBID    0X14
#define STU_WEBSOCKET_CMD_RELIVE    0X18

#define STU_WEBSOCKET_CMD_UPGRADE   0X21
#define STU_WEBSOCKET_CMD_DEMOTE    0X22

#define STU_WEBSOCKET_CMD_DISMISS   0X41
#define STU_WEBSOCKET_CMD_ACTIVE    0X42

typedef struct stu_websocket_frame_s stu_websocket_frame_t;

struct stu_websocket_frame_s {
	u_char                 FIN:1;
	u_char                 RSV1:1;
	u_char                 RSV2:1;
	u_char                 RSV3:1;
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

	stu_int_t              command;
	stu_int_t              result;

	// used for parsing request.
	stu_uint_t             state;
} stu_websocket_request_t;

void stu_websocket_wait_request_handler(stu_event_t *rev);
void stu_websocket_request_handler(stu_event_t *wev);

stu_websocket_request_t *stu_websocket_create_request(stu_connection_t *c);
void stu_websocket_process_request(stu_websocket_request_t *r);

void stu_websocket_finalize_request(stu_websocket_request_t *r, stu_int_t rc);

void stu_websocket_close_request(stu_websocket_request_t *r, stu_int_t rc);
void stu_websocket_free_request(stu_websocket_request_t *r, stu_int_t rc);
void stu_websocket_close_connection(stu_connection_t *c);

void stu_websocket_empty_handler(stu_event_t *ev);
void stu_websocket_request_empty_handler(stu_websocket_request_t *r);

#endif /* STU_WEBSOCKET_REQUEST_H_ */
