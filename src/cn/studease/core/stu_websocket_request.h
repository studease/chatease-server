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

typedef struct stu_websocket_frame_s stu_websocket_frame_t;

struct stu_websocket_frame_s {
	unsigned               FIN:1;
	unsigned               RSV:3;
	unsigned               opcode:4;
	unsigned               mask:1;
	unsigned               payload_len:7;
	uint64_t               extended;
	u_char                 masking_key[4];
	stu_buf_t             *payload_data;

	stu_websocket_frame_t *next;
};

typedef struct {
	stu_connection_t      *connection;
	stu_websocket_frame_t *frame;
} stu_websocket_request_t;

#endif /* STU_WEBSOCKET_REQUEST_H_ */
