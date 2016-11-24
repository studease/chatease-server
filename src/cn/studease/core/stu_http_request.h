/*
 * stu_http_request.h
 *
 *  Created on: 2016-9-14
 *      Author: Tony Lau
 */

#ifndef STU_HTTP_REQUEST_H_
#define STU_HTTP_REQUEST_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_HTTP_SWITCHING_PROTOCOLS       101
#define STU_HTTP_OK                        200
#define STU_HTTP_MOVED_TEMPORARILY         302

#define STU_HTTP_BAD_REQUEST               400
#define STU_HTTP_UNAUTHORIZED              401
#define STU_HTTP_FORBIDDEN                 403
#define STU_HTTP_NOT_FOUND                 404
#define STU_HTTP_NOT_ALLOWED               405
#define STU_HTTP_REQUEST_TIME_OUT          408

#define STU_HTTP_INTERNAL_SERVER_ERROR     500
#define STU_HTTP_NOT_IMPLEMENTED           501
#define STU_HTTP_SERVICE_UNAVAILABLE       503

typedef struct {
	stu_str_t                   name;
	stu_uint_t                  offset;
	stu_http_header_handler_pt  handler;
} stu_http_header_t;

typedef struct {
	stu_str_t                   name;
	stu_uint_t                  offset;
} stu_http_header_out_t;

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

struct stu_http_request_s {
	stu_connection_t   *connection;

	stu_buf_t           header;
	stu_buf_t          *request_body;

	stu_http_headers_t  headers_in;
	stu_http_headers_t  headers_out;
	stu_buf_t          *response_body;
};

#endif /* STU_HTTP_REQUEST_H_ */
