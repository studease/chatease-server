/*
 * stu_websocket.c
 *
 *  Created on: 2016-9-14
 *      Author: Tony Lau
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "stu_config.h"
#include "stu_core.h"

stu_int_t
stu_websocket_add_listen(stu_config_t *cf) {
	stu_socket_t        fd;
	stu_connection_t   *c;
	stu_event_t         ev;
	struct sockaddr_in  sa;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		stu_log_error(stu_errno, "Failed to create websocket server fd.");
		return STU_ERROR;
	}

	if (stu_nonblocking(fd) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while setting websocket server fd.");
		return STU_ERROR;
	}

	c = stu_connection_get(fd);
	if (c == NULL) {
		stu_log_error(0, "Failed to get websocket server connection.");
		return STU_ERROR;
	}

	ev.active = FALSE;
	ev.data = (void *) c;
	ev.type = 0;
	ev.handler = NULL;
	if (stu_epoll_add_event(&ev, STU_READ_EVENT) == STU_ERROR) {
		stu_log_error(0, "Failed to add websocket server event.");
		return STU_ERROR;
	}

	bzero(&(sa.sin_zero), 8);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htons(INADDR_ANY);
	sa.sin_port = htons(cf->port);

	if (bind(fd, (struct sockaddr*)&sa, sizeof(sa))) {
		stu_log_error(stu_errno, "Failed to bind websocket server fd.");
		return STU_ERROR;
	}

	if (listen(fd, cf->port)) {
		stu_log_error(stu_errno, "Failed to listen websocket server port %d.\n", cf->port);
		return STU_ERROR;
	}

	return STU_OK;
}

