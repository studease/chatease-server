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
#define STU_WEBSOCKET_OPCODE_BINARY         0x2
#define STU_WEBSOCKET_OPCODE_CLOSE          0x8
#define STU_WEBSOCKET_OPCODE_PING           0x9
#define STU_WEBSOCKET_OPCODE_PONG           0xA

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

	stu_int_t              status;

	// used for parsing request.
	stu_uint_t             state;
	stu_websocket_frame_t *frame;
} stu_websocket_request_t;

void stu_websocket_wait_request_handler(stu_event_t *rev);
void stu_websocket_request_handler(stu_event_t *wev);

stu_websocket_request_t *stu_websocket_create_request(stu_connection_t *c);
void stu_websocket_process_request(stu_websocket_request_t *r);

void stu_websocket_finalize_request(stu_websocket_request_t *r, stu_int_t rc, stu_double_t req);
u_char *stu_websocket_encode_frame(u_char opcode, u_char *buf, uint64_t len, stu_int_t *extened);

void stu_websocket_close_request(stu_websocket_request_t *r, stu_int_t rc);
void stu_websocket_free_request(stu_websocket_request_t *r, stu_int_t rc);
void stu_websocket_close_connection(stu_connection_t *c);

void stu_websocket_empty_handler(stu_event_t *ev);
void stu_websocket_request_empty_handler(stu_websocket_request_t *r);

#endif /* STU_WEBSOCKET_REQUEST_H_ */
