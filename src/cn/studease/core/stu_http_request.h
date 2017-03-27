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

#define STU_HTTP_REQUEST_DEFAULT_SIZE      1024
#define STU_HTTP_LC_HEADER_LEN             32

#define STU_HTTP_VERSION_10                10
#define STU_HTTP_VERSION_11                11

#define STU_HTTP_UNKNOWN                   0x0001
#define STU_HTTP_GET                       0x0002
#define STU_HTTP_POST                      0x0008

#define STU_HTTP_SWITCHING_PROTOCOLS       101
#define STU_HTTP_OK                        200
#define STU_HTTP_NOTIFICATION              209
#define STU_HTTP_MOVED_TEMPORARILY         302

#define STU_HTTP_BAD_REQUEST               400
#define STU_HTTP_UNAUTHORIZED              401
#define STU_HTTP_FORBIDDEN                 403
#define STU_HTTP_NOT_FOUND                 404
#define STU_HTTP_METHOD_NOT_ALLOWED        405
#define STU_HTTP_REQUEST_TIMEOUT           408
#define STU_HTTP_CONFLICT                  409

#define STU_HTTP_INTERNAL_SERVER_ERROR     500
#define STU_HTTP_NOT_IMPLEMENTED           501
#define STU_HTTP_BAD_GATEWAY               502
#define STU_HTTP_SERVICE_UNAVAILABLE       503
#define STU_HTTP_GATEWAY_TIMEOUT           504

#define STU_HTTP_CONNECTION_CLOSE          0
#define STU_HTTP_CONNECTION_KEEP_ALIVE     1
#define STU_HTTP_CONNECTION_UPGRADE        2

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
	stu_list_t       headers;

	stu_table_elt_t *host;
	stu_table_elt_t *user_agent;

	stu_table_elt_t *accept;
	stu_table_elt_t *accept_language;
#if (STU_HTTP_GZIP)
	stu_table_elt_t *accept_encoding;
#endif

	stu_table_elt_t *content_type;
	stu_table_elt_t *content_length;

	stu_table_elt_t *sec_websocket_key;
	stu_table_elt_t *sec_websocket_key1;
	stu_table_elt_t *sec_websocket_key2;
	stu_table_elt_t *sec_websocket_key3;
	stu_table_elt_t *sec_websocket_version;
	stu_table_elt_t *sec_websocket_extensions;
	stu_table_elt_t *upgrade;

	stu_table_elt_t *connection;

	stu_int_t        content_length_n;
	uint8_t          connection_type;
} stu_http_headers_in_t;

typedef struct {
	stu_list_t       headers;

	stu_uint_t       status;
	stu_str_t        status_line;

	stu_table_elt_t *server;

	stu_table_elt_t *content_type;
	stu_table_elt_t *content_length;
	stu_table_elt_t *content_encoding;

	stu_table_elt_t *sec_websocket_accept;
	stu_table_elt_t *sec_websocket_extensions;
	stu_table_elt_t *upgrade;

	stu_table_elt_t *connection;
	stu_table_elt_t *date;

	stu_int_t        content_length_n;
	uint8_t          connection_type;
} stu_http_headers_out_t;

struct stu_http_request_s {
	stu_connection_t       *connection;

	stu_short_t             method;
	uint8_t                 http_version;

	stu_str_t               request_line;
	stu_str_t               uri;
	stu_str_t               args;

	stu_buf_t              *header_in;
	stu_buf_t              *request_body;

	stu_http_headers_in_t   headers_in;
	stu_http_headers_out_t  headers_out;
	stu_buf_t               response_body;

	// used for parsing request.
	uint8_t                 state;
	stu_uint_t              header_hash;
	stu_uint_t              lowcase_index;
	u_char                  lowcase_header[STU_HTTP_LC_HEADER_LEN];

	stu_bool_t              invalid_header;

	u_char                 *header_name_start;
	u_char                 *header_name_end;
	u_char                 *header_start;
	u_char                 *header_end;
};

void stu_http_wait_request_handler(stu_event_t *rev);

stu_http_request_t *stu_http_create_request(stu_connection_t *c);
void stu_http_process_request(stu_http_request_t *r);

void stu_http_finalize_request(stu_http_request_t *r, stu_int_t rc);

void stu_http_close_request(stu_http_request_t *r, stu_int_t rc);
void stu_http_free_request(stu_http_request_t *r, stu_int_t rc);
void stu_http_close_connection(stu_connection_t *c);

void stu_http_empty_handler(stu_event_t *ev);
void stu_http_request_empty_handler(stu_http_request_t *r);

#endif /* STU_HTTP_REQUEST_H_ */
