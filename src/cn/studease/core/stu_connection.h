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

#define STU_CONNECTIONS_PER_PAGE 2048
#define STU_CONNECTION_MAX_PAGE  32

#define STU_CONNECTION_ERROR_NONE      0
#define STU_CONNECTION_ERROR_TIMEDOUT  0x01
#define STU_CONNECTION_ERROR_INNER     0x02
#define STU_CONNECTION_ERROR_DESTROYED 0x04

typedef struct stu_connection_page_s stu_connection_page_t;

typedef struct {
	stu_queue_t            queue;
	stu_spinlock_t         lock;

	stu_connection_page_t *page;
	stu_base_pool_t       *pool;

	stu_socket_t           fd;
	stu_user_t             user;

	void                  *data;
	stu_event_t           *read;
	stu_event_t           *write;

	stu_uint_t             error;  // timed out, inner error, destroyed
} stu_connection_t;

struct stu_connection_page_s {
	stu_queue_t            queue;
	stu_spinlock_t         lock;
	stu_pool_data_t        data;

	stu_connection_t       connections;
	stu_uint_t             length;

	stu_connection_t       free;
};

typedef struct {
	stu_spinlock_t         lock;
	stu_connection_page_t  pages;
} stu_connection_pool_t;

stu_connection_pool_t *stu_connection_pool_create(stu_pool_t *pool);

stu_connection_t *stu_connection_get(stu_socket_t s);
void stu_connection_free(stu_connection_t *c);
void stu_connection_close(stu_connection_t *c);

#endif /* STU_CONNECTION_H_ */
