/*
 * stu_http.h
 *
 *  Created on: 2016-11-24
 *      Author: Tony Lau
 */

#ifndef STU_HTTP_H_
#define STU_HTTP_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_HTTP_HEADERS_MAX_SIZE  512

typedef struct stu_http_request_s stu_http_request_t;

typedef stu_int_t (*stu_http_header_handler_pt)(stu_http_request_t *r, stu_table_elt_t *h, stu_uint_t offset);

#include "stu_http_request.h"
#include "stu_http_parse.h"
#include "stu_websocket_request.h"

stu_int_t stu_http_add_listen(stu_config_t *cf);

#endif /* STU_HTTP_H_ */
