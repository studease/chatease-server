/*
 * stu_flash.c
 *
 *  Created on: 2017-6-27
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static stu_socket_t     stu_corsfd;

stu_str_t  STU_FLASH_POLICY_REQUEST = stu_string(
		"<policy-file-request/>\0"
	);

stu_str_t  STU_FLASH_POLICY_FILE = stu_string(
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<cross-domain-policy>"
			"<allow-access-from domain=\"*\" />"
		"</cross-domain-policy>"
	);

static void stu_flash_server_handler(stu_event_t *ev);
static void stu_flash_wait_request_handler(stu_event_t *rev);


stu_int_t
stu_flash_add_listen(stu_config_t *cf) {
	stu_connection_t   *c;
	struct sockaddr_in  sa;

	stu_corsfd = socket(AF_INET, SOCK_STREAM, 0);
	if (stu_corsfd == -1) {
		stu_log_error(stu_errno, "Failed to create flash server fd.");
		return STU_ERROR;
	}

	if (stu_nonblocking(stu_corsfd) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while setting flash server fd.");
		return STU_ERROR;
	}

	c = stu_connection_get(stu_corsfd);
	if (c == NULL) {
		stu_log_error(0, "Failed to get flash server connection.");
		return STU_ERROR;
	}

	c->read.handler = stu_flash_server_handler;
	if (stu_event_add(&c->read, STU_READ_EVENT, 0) == STU_ERROR) {
		stu_log_error(0, "Failed to add flash server event.");
		return STU_ERROR;
	}

	bzero(&(sa.sin_zero), 8);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htons(INADDR_ANY);
	sa.sin_port = htons(STU_FLASH_CORS_PORT);

	stu_log("Binding sockaddr(%hu)...", STU_FLASH_CORS_PORT);
	if (bind(stu_corsfd, (struct sockaddr*)&sa, sizeof(sa))) {
		stu_log_error(stu_errno, "Failed to bind flash server fd.");
		return STU_ERROR;
	}

	stu_log("Listening on port %d.", STU_FLASH_CORS_PORT);
	if (listen(stu_corsfd, STU_FLASH_CORS_PORT)) {
		stu_log_error(stu_errno, "Failed to listen flash server port %hu.\n", STU_FLASH_CORS_PORT);
		return STU_ERROR;
	}

	return STU_OK;
}


static void
stu_flash_server_handler(stu_event_t *ev) {
	stu_socket_t        fd;
	struct sockaddr_in  sa;
	socklen_t           socklen;
	stu_int_t           err;
	stu_connection_t   *c;

	socklen = sizeof(sa);

again:

	fd = accept(stu_corsfd, (struct sockaddr*)&sa, &socklen);
	if (fd == -1) {
		err = stu_errno;
		if (err == EAGAIN) {
			stu_log_debug(4, "Already accepted by other threads: errno=%d.", err);
			return;
		}

		if (err == EINTR) {
			stu_log_debug(4, "accept trying again: errno=%d.", err);
			goto again;
		}

		stu_log_error(err, "Failed to accept!");
		return;
	}

	if (stu_nonblocking(fd) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while setting flash client fd.");
		return;
	}

	c = stu_connection_get(fd);
	if (c == NULL) {
		stu_log_error(0, "Failed to get flash client connection.");
		return;
	}

	c->read.handler = stu_flash_wait_request_handler;
	if (stu_event_add(&c->read, STU_READ_EVENT, STU_CLEAR_EVENT) == STU_ERROR) {
		stu_log_error(0, "Failed to add flash client read event.");
		return;
	}
}


static void
stu_flash_wait_request_handler(stu_event_t *rev) {
	stu_connection_t *c;
	stu_int_t         n, err;

	c = (stu_connection_t *) rev->data;

	stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) STU_SOCKET_INVALID) {
		goto done;
	}

	if (c->buffer.start == NULL) {
		c->buffer.start = (u_char *) stu_base_palloc(c->pool, STU_HTTP_REQUEST_DEFAULT_SIZE);
		c->buffer.end = c->buffer.start + STU_HTTP_REQUEST_DEFAULT_SIZE;
	}
	c->buffer.last = c->buffer.start;
	stu_memzero(c->buffer.start, STU_HTTP_REQUEST_DEFAULT_SIZE);

again:

	n = recv(c->fd, c->buffer.start, STU_HTTP_REQUEST_DEFAULT_SIZE, 0);
	if (n == -1) {
		err = stu_errno;
		if (err == EAGAIN) {
			stu_log_debug(4, "Already received by other threads: fd=%d, errno=%d.", c->fd, err);
			goto done;
		}

		if (err == EINTR) {
			stu_log_debug(4, "recv trying again: fd=%d, errno=%d.", c->fd, err);
			goto again;
		}

		stu_log_error(err, "Failed to recv data: fd=%d.", c->fd);
		goto failed;
	}

	if (n == 0) {
		stu_log_debug(4, "Remote client has closed connection: fd=%d.", c->fd);
		goto failed;
	}

	stu_log_debug(4, "recv: fd=%d, bytes=%d.", c->fd, n); //str=\n%s, c->buffer.start

	if (stu_strncmp(c->buffer.start, STU_FLASH_POLICY_REQUEST.data, STU_FLASH_POLICY_REQUEST.len) == 0) {
		n = send(c->fd, STU_FLASH_POLICY_FILE.data, STU_FLASH_POLICY_FILE.len, 0);
		if (n == -1) {
			stu_log_debug(4, "Failed to send policy file: fd=%d.", c->fd);
			goto failed;
		}

		stu_log_debug(4, "sent policy file: fd=%d, bytes=%d.", c->fd, n);

		goto done; // Todo: close in 3 sec
	}

failed:

	c->read.active = 0;
	stu_event_del(&c->read, STU_READ_EVENT, 0);

	stu_connection_close(c);

done:

	stu_spin_unlock(&c->lock);
}
