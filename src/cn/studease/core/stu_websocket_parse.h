/*
 * stu_websocket_parse.h
 *
 *  Created on: 2016-12-5
 *      Author: Tony Lau
 */

#ifndef STU_WEBSOCKET_PARSE_H_
#define STU_WEBSOCKET_PARSE_H_

#include "stu_config.h"
#include "stu_core.h"

stu_int_t stu_websocket_parse_frame(stu_websocket_request_t *r, stu_buf_t *b);

#endif /* STU_WEBSOCKET_PARSE_H_ */
