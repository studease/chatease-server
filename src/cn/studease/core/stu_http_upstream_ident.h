/*
 * stu_http_upstream_ident.h
 *
 *  Created on: 2017-3-14
 *      Author: Tony Lau
 */

#ifndef STU_HTTP_UPSTREAM_IDENT_H_
#define STU_HTTP_UPSTREAM_IDENT_H_

#include "stu_config.h"
#include "stu_core.h"

//#define STU_HTTP_UPSTREAM_IDENT_TOKEN_MAX_LEN 128

stu_int_t  stu_http_upstream_ident_generate_request(stu_connection_t *c);
stu_int_t  stu_http_upstream_ident_analyze_response(stu_connection_t *c);

#endif /* STU_UPSTREAM_IDENT_H_ */
