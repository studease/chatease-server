/*
 * stu_connection.c
 *
 *  Created on: 2016-9-26
 *      Author: Tony Lau
 */

#include <unistd.h>
#include <sys/socket.h>
#include "stu_config.h"
#include "stu_core.h"
#include "stu_event.h"


static void stu_connection_init(stu_connection_t *c, stu_socket_t s);


stu_connection_t *
stu_connection_get(stu_socket_t s) {
	stu_connection_t      *c;

	c = stu_calloc(sizeof(stu_connection_t));
	if (c == NULL) {
		return NULL;
	}

	stu_connection_init(c, s);

	c->read.data = c->write.data = (void *) c;
	c->read.type = c->write.type = 0;
	c->read.active = 0;
	c->write.active = 1;

	stu_log_debug(2, "Got connection: c=%p, fd=%d.", c, c->fd);

	return c;
}

void
stu_connection_free(stu_connection_t *c) {
	stu_socket_t  fd;

	fd = c->fd;
	c->fd = (stu_socket_t) -1;
	c->read.active = c->write.active = 0;

	stu_upstream_cleanup(c);
	stu_free((void *) c);

	stu_log_debug(2, "Freed connection: c=%p, fd=%d.", c, fd);
}

void
stu_connection_close(stu_connection_t *c) {
	/*if (c->fd == (stu_socket_t) -1) {
		stu_log_debug(2, "connection already closed.");
		return;
	}*/

	stu_close_socket(c->fd);
	stu_connection_free(c);
}


static void
stu_connection_init(stu_connection_t *c, stu_socket_t s) {
	//stu_mutex_init(&c->lock);

	c->fd = s;

	stu_user_init(&c->user, NULL, NULL);

	c->buffer.start = c->buffer.last = c->buffer.end = NULL;
	c->data = NULL;
	c->read.active = c->write.active = 0;

	c->upstream = NULL;

	c->error = STU_CONNECTION_ERROR_NONE;
}

