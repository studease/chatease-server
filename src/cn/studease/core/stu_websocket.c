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

static stu_socket_t  stu_wsfd;
extern stu_cycle_t  *stu_cycle;

static void stu_websocket_server_handler(stu_event_t *ev);

static void stu_websocket_client_in_handler(stu_event_t *ev);
static void stu_websocket_client_out_handler(stu_event_t *ev);


stu_int_t
stu_websocket_add_listen(stu_config_t *cf) {
	stu_connection_t   *c;
	struct sockaddr_in  sa;

	stu_wsfd = socket(AF_INET, SOCK_STREAM, 0);
	if (stu_wsfd == -1) {
		stu_log_error(stu_errno, "Failed to create websocket server fd.");
		return STU_ERROR;
	}

	if (stu_nonblocking(stu_wsfd) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while setting websocket server fd.");
		return STU_ERROR;
	}

	c = stu_connection_get(stu_wsfd);
	if (c == NULL) {
		stu_log_error(0, "Failed to get websocket server connection.");
		return STU_ERROR;
	}

	c->read->handler = stu_websocket_server_handler;
	if (stu_epoll_add_event(c->read, STU_READ_EVENT) == STU_ERROR) {
		stu_log_error(0, "Failed to add websocket server event.");
		return STU_ERROR;
	}

	bzero(&(sa.sin_zero), 8);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htons(INADDR_ANY);
	sa.sin_port = htons(cf->port);

	stu_log("Binding sockaddr...");
	if (bind(stu_wsfd, (struct sockaddr*)&sa, sizeof(sa))) {
		stu_log_error(stu_errno, "Failed to bind websocket server fd.");
		return STU_ERROR;
	}

	stu_log("Listening on port %d.", cf->port);
	if (listen(stu_wsfd, cf->port)) {
		stu_log_error(stu_errno, "Failed to listen websocket server port %d.\n", cf->port);
		return STU_ERROR;
	}

	return STU_OK;
}

static void
stu_websocket_server_handler(stu_event_t *ev) {
	stu_socket_t        fd;
	struct sockaddr_in  sa;
	socklen_t           socklen;
	stu_connection_t   *c;

	socklen = sizeof(sa);

	fd = accept(stu_wsfd, (struct sockaddr*)&sa, &socklen);
	if (fd == -1) {
		stu_log_error(stu_errno, "Failed to accept!");
		return;
	}

	if (stu_nonblocking(fd) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while setting websocket client fd.");
		return;
	}

	c = stu_connection_get(fd);
	if (c == NULL) {
		stu_log_error(0, "Failed to get websocket client connection.");
		return;
	}

	c->read->handler = stu_websocket_client_in_handler;
	if (stu_epoll_add_event(c->read, STU_READ_EVENT) == STU_ERROR) {
		stu_log_error(0, "Failed to add websocket client read event.");
		return;
	}

	c->write->handler = stu_websocket_client_out_handler;
	if (stu_epoll_add_event(c->write, STU_WRITE_EVENT) == STU_ERROR) {
		stu_log_error(0, "Failed to add websocket client write event.");
		return;
	}
}

static void
stu_websocket_client_in_handler(stu_event_t *ev) {
	stu_connection_t *c;
	u_char            buf[2048];
	stu_int_t         n;

	c = (stu_connection_t *) ev->data;

	n = recv(c->fd, buf, 2048, 0);
	if (n < 0) {
		if (stu_errno == EAGAIN) {
			return;
		}

		stu_log_debug(0, "Failed to receive data: fd=%d.", c->fd);
		stu_ram_free(stu_cycle->ram_pool, (void *) c->pool);
		stu_connection_free(c);
		return;
	}

	if (n == 0) {
		stu_log_debug(0, "Remote client has closed connection.");
		stu_connection_close(c);
		return;
	}

	stu_log_debug(0, "%d: %s", c->fd, buf);
}

static void
stu_websocket_client_out_handler(stu_event_t *ev) {

}

