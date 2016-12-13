/*
 * stu_http.c
 *
 *  Created on: 2016-11-24
 *      Author: Tony Lau
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "stu_config.h"
#include "stu_core.h"

extern stu_cycle_t       *stu_cycle;
extern stu_http_header_t  stu_http_headers_in[];

stu_hash_t                stu_http_headers_in_hash;
static stu_socket_t       stu_httpfd;

static stu_int_t stu_http_init_headers_in_hash(stu_config_t *cf);
static void stu_http_server_handler(stu_event_t *ev);


stu_int_t
stu_http_add_listen(stu_config_t *cf) {
	int                 reuseaddr;
	stu_connection_t   *c;
	struct sockaddr_in  sa;

	if (stu_http_init_headers_in_hash(cf) == STU_ERROR) {
		stu_log_error(0, "Failed to init http headers in hash.");
		return STU_ERROR;
	}

	stu_httpfd = socket(AF_INET, SOCK_STREAM, 0);
	if (stu_httpfd == -1) {
		stu_log_error(stu_errno, "Failed to create http server fd.");
		return STU_ERROR;
	}

	reuseaddr = 1;
	if (setsockopt(stu_httpfd, SOL_SOCKET, SO_REUSEADDR, (const void *) &reuseaddr, sizeof(int)) == -1) {
		stu_log_error(stu_errno, "setsockopt(SO_REUSEADDR) failed while setting http server fd.");
		return STU_ERROR;
	}

	if (stu_nonblocking(stu_httpfd) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while setting http server fd.");
		return STU_ERROR;
	}

	c = stu_connection_get(stu_httpfd);
	if (c == NULL) {
		stu_log_error(0, "Failed to get http server connection.");
		return STU_ERROR;
	}

	c->read->handler = stu_http_server_handler;
	if (stu_epoll_add_event(c->read, STU_READ_EVENT) == STU_ERROR) {
		stu_log_error(0, "Failed to add http server event.");
		return STU_ERROR;
	}

	bzero(&(sa.sin_zero), 8);
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htons(INADDR_ANY);
	sa.sin_port = htons(cf->port);

	stu_log("Binding sockaddr...");
	if (bind(stu_httpfd, (struct sockaddr*)&sa, sizeof(sa))) {
		stu_log_error(stu_errno, "Failed to bind http server fd.");
		return STU_ERROR;
	}

	stu_log("Listening on port %d.", cf->port);
	if (listen(stu_httpfd, cf->port)) {
		stu_log_error(stu_errno, "Failed to listen http server port %d.\n", cf->port);
		return STU_ERROR;
	}

	return STU_OK;
}

static void
stu_http_server_handler(stu_event_t *ev) {
	stu_socket_t        fd;
	struct sockaddr_in  sa;
	socklen_t           socklen;
	stu_int_t           err, retried;
	stu_connection_t   *c;

	socklen = sizeof(sa);
	retried = 0;

again:

	fd = accept(stu_httpfd, (struct sockaddr*)&sa, &socklen);
	if (fd == -1) {
		err = stu_errno;
		if (err == EAGAIN || err == EINTR) {
			if (retried++ >= 5) {
				stu_log_debug(0, "accept aborted: errno=%d.", err);
				return;
			}

			stu_log_debug(0, "accept trying again: errno=%d.", err);
			goto again;
		}

		stu_log_error(err, "Failed to accept!");
		return;
	}

	if (stu_nonblocking(fd) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while setting http client fd.");
		return;
	}

	c = stu_connection_get(fd);
	if (c == NULL) {
		stu_log_error(0, "Failed to get http client connection.");
		return;
	}

	c->read->handler = stu_http_wait_request_handler;
	if (stu_epoll_add_event(c->read, STU_READ_EVENT|EPOLLET) == STU_ERROR) {
		stu_log_error(0, "Failed to add http client read event.");
		return;
	}
}

static stu_int_t
stu_http_init_headers_in_hash(stu_config_t *cf) {
	stu_http_header_t *header;
	stu_str_t          lc;
	u_char             data[STU_HTTP_LC_HEADER_LEN];

	if (stu_hash_init(&stu_http_headers_in_hash, NULL, STU_HTTP_HEADERS_MAX_SIZE, stu_cycle->pool,
			(stu_hash_palloc_pt) stu_pcalloc, NULL) == STU_ERROR) {
		return STU_ERROR;
	}

	for (header = stu_http_headers_in; header->name.len; header++) {
		stu_strlow(data, header->name.data, header->name.len);

		lc.data = data;
		lc.len = header->name.len;

		if (stu_hash_insert(&stu_http_headers_in_hash, &lc, header, STU_HASH_LOWCASE_KEY) == STU_ERROR) {
			return STU_ERROR;
		}
	}

	return STU_OK;
}

