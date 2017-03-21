/*
 * stu_upstream_ident.h
 *
 *  Created on: 2017-3-14
 *      Author: Tony Lau
 */

#ifndef STU_UPSTREAM_IDENT_H_
#define STU_UPSTREAM_IDENT_H_

#include "stu_config.h"
#include "stu_core.h"

void stu_upstream_ident_read_handler(stu_event_t *ev);
void stu_upstream_ident_write_handler(stu_event_t *ev);

stu_int_t stu_upstream_ident_process_response(stu_connection_t *c);
stu_int_t stu_upstream_ident_analyze_response(stu_connection_t *c);
void stu_upstream_ident_finalize_handler(stu_connection_t *c, stu_int_t rc);

#endif /* STU_UPSTREAM_IDENT_H_ */
