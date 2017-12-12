/*
 * stu_connection.h
 *
 *  Created on: 2016-9-9
 *      Author: Tony Lau
 */

#ifndef STU_CONNECTION_H_
#define STU_CONNECTION_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_CONNECTIONS_PER_PAGE   4096
#define STU_CONNECTION_PAGE_MAX_N  16

#define STU_CONNECTION_ERROR_NONE      0x00
#define STU_CONNECTION_ERROR_TIMEDOUT  0x01
#define STU_CONNECTION_ERROR_INNER     0x02
#define STU_CONNECTION_ERROR_DESTROYED 0x04

typedef struct {
	stu_queue_t            queue;
	stu_mutex_t            lock;

	stu_socket_t           fd;
	stu_user_t             user;

	stu_buf_t              buffer;
	void                  *data;
	stu_event_t            read;
	stu_event_t            write;

	stu_upstream_t        *upstream;

	stu_uint_t             error;  // timed out, inner error, destroyed
} stu_connection_t;

stu_connection_t *stu_connection_get(stu_socket_t s);
void stu_connection_free(stu_connection_t *c);
void stu_connection_close(stu_connection_t *c);

stu_int_t stu_channel_insert(stu_str_t *ch, stu_connection_t *c);
stu_int_t stu_channel_insert_locked(stu_channel_t *ch, stu_connection_t *c);

void stu_channel_remove(stu_channel_t *ch, stu_connection_t *c);
void stu_channel_remove_locked(stu_channel_t *ch, stu_connection_t *c);

#endif /* STU_CONNECTION_H_ */
