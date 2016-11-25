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

void stu_websocket_wait_request_handler(stu_event_t *rev);
void stu_websocket_request_handler(stu_event_t *wev);

stu_websocket_request_t *stu_websocket_create_request(stu_connection_t *c);
void stu_websocket_process_request(stu_websocket_request_t *r);

void stu_websocket_finalize_request(stu_websocket_request_t *r, stu_int_t rc);

void stu_websocket_close_request(stu_websocket_request_t *r, stu_int_t rc);
void stu_websocket_free_request(stu_websocket_request_t *r, stu_int_t rc);
void stu_websocket_close_connection(stu_connection_t *c);

void stu_websocket_empty_handler(stu_event_t *wev);
void stu_websocket_request_empty_handler(stu_websocket_request_t *r);

#endif /* STU_WEBSOCKET_REQUEST_H_ */
