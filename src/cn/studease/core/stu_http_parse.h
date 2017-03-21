/*
 * stu_http_parse.h
 *
 *  Created on: 2016-11-28
 *      Author: root
 */

#ifndef STU_HTTP_PARSE_H_
#define STU_HTTP_PARSE_H_

#include "stu_config.h"
#include "stu_core.h"

stu_int_t stu_http_parse_request_line(stu_http_request_t *r, stu_buf_t *b);
stu_int_t stu_http_parse_status_line(stu_http_request_t *r, stu_buf_t *b);
stu_int_t stu_http_parse_header_line(stu_http_request_t *r, stu_buf_t *b, stu_uint_t allow_underscores);

stu_int_t stu_http_parse_uri(stu_http_request_t *r);

void stu_http_split_args(stu_http_request_t *r, stu_str_t *uri, stu_str_t *args);
stu_int_t stu_http_arg(stu_http_request_t *r, u_char *name, size_t len, stu_str_t *value);

#endif /* STU_HTTP_PARSE_H_ */
