/*
 * stu_http_upstream.h
 *
 *  Created on: 2017-6-19
 *      Author: Tony Lau
 */

#ifndef STU_HTTP_UPSTREAM_H_
#define STU_HTTP_UPSTREAM_H_

#include "stu_config.h"
#include "stu_core.h"

void stu_http_upstream_read_handler(stu_event_t *ev);
void stu_http_upstream_write_handler(stu_event_t *ev);

void      *stu_http_upstream_create_request(stu_connection_t *c);
stu_int_t  stu_http_upstream_reinit_request(stu_connection_t *c);
stu_int_t  stu_http_upstream_generate_request(stu_connection_t *c);

stu_int_t  stu_http_upstream_process_response(stu_connection_t *c);
stu_int_t  stu_http_upstream_analyze_response(stu_connection_t *c);
void       stu_http_upstream_finalize_handler(stu_connection_t *c, stu_int_t rc);

void       stu_http_upstream_cleanup(stu_connection_t *c);

#endif /* STU_HTTP_UPSTREAM_H_ */
