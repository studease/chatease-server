/*
 * stu_request.h
 *
 *  Created on: 2016-9-14
 *      Author: Tony Lau
 */

#ifndef STU_REQUEST_H_
#define STU_REQUEST_H_

#include "stu_config.h"
#include "stu_core.h"

typedef struct {
	stu_uint_t       status;
	stu_buf_t        status_line;

	stu_table_elt_t *host;
	stu_table_elt_t *server;
	stu_table_elt_t *user_agent;

	stu_table_elt_t *accept;
	stu_table_elt_t *accept_language;
	stu_table_elt_t *accept_encoding;

	stu_table_elt_t *content_type;
	stu_table_elt_t *content_length;

	stu_table_elt_t *origin;
	stu_table_elt_t *sec_websocket_extensions;
	stu_table_elt_t *sec_websocket_key;
	stu_table_elt_t *sec_websocket_accept;
	stu_table_elt_t *sec_websocket_version;
	stu_table_elt_t *upgrade;
	stu_table_elt_t *connection;

	stu_table_elt_t *date;
} stu_http_headers_t;

typedef struct {
	stu_connection_t   *connection;
	stu_slab_pool_t    *pool;

	stu_buf_t           request_header;
	stu_buf_t          *request_body;

	stu_http_headers_t  headers_in;
	stu_http_headers_t  headers_out;
	stu_buf_t          *response_body;
} stu_http_request_t;

typedef struct stu_websocket_frame_s stu_websocket_frame_t;

struct stu_websocket_frame_s {
	unsigned               FIN:1;
	unsigned               RSV:3;
	unsigned               opcode:4;
	unsigned               mask:1;
	unsigned               payload_len:7;
	uint64_t               extended;
	char                   masking_key[4];
	stu_buf_t             *payload_data;

	stu_atomic_t           lock;
	stu_websocket_frame_t *next;
};

typedef struct {
	stu_spinlock_t         lock;

	stu_connection_t      *connection;
	stu_slab_pool_t       *pool;

	stu_websocket_frame_t  frame;
} stu_websocket_request_t;

stu_int_t stu_http_init_request(stu_http_request_t *r);
void stu_http_finalize_request(stu_http_request_t *r);



#endif /* STU_REQUEST_H_ */
