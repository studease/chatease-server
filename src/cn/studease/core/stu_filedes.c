/*
 * stu_filedes.c
 *
 *  Created on: 2016-9-21
 *      Author: Tony Lau
 */

#include <sys/socket.h>
#include <unistd.h>
#include "stu_config.h"
#include "stu_core.h"

stu_int_t
stu_filedes_write(stu_socket_t s, stu_filedes_t *fds, size_t size) {
	ssize_t             n;
	stu_int_t           err;
	struct iovec        iov[1];
	struct msghdr       msg;

	union {
		struct cmsghdr  cm;
		char            space[CMSG_SPACE(sizeof(int))];
	} cmsg;

	if (fds->fd == -1) {
		msg.msg_control = NULL;
		msg.msg_controllen = 0;

	} else {
		msg.msg_control = (caddr_t) &cmsg;
		msg.msg_controllen = sizeof(cmsg);

		stu_memzero(&cmsg, sizeof(cmsg));

		cmsg.cm.cmsg_len = CMSG_LEN(sizeof(int));
		cmsg.cm.cmsg_level = SOL_SOCKET;
		cmsg.cm.cmsg_type = SCM_RIGHTS;

		memcpy(CMSG_DATA(&cmsg.cm), &fds->fd, sizeof(int));
	}

	msg.msg_flags = 0;

	iov[0].iov_base = (char *) fds;
	iov[0].iov_len = size;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	n = sendmsg(s, &msg, 0);

	if (n == -1) {
		err = stu_errno;
		if (err == EAGAIN) {
			return STU_AGAIN;
		}

		stu_log_error(err, "sendmsg() failed");
		return STU_ERROR;
	}

	return STU_OK;
}

stu_int_t
stu_filedes_read(stu_socket_t s, stu_filedes_t *fds, size_t size) {
	ssize_t             n;
	stu_int_t           err;
	struct iovec        iov[1];
	struct msghdr       msg;

	union {
		struct cmsghdr  cm;
		char            space[CMSG_SPACE(sizeof(int))];
	} cmsg;

	iov[0].iov_base = (char *) fds;
	iov[0].iov_len = size;

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	msg.msg_control = (caddr_t) &cmsg;
	msg.msg_controllen = sizeof(cmsg);

	n = recvmsg(s, &msg, 0);

	if (n == -1) {
		err = stu_errno;
		stu_log_error(err, "recvmsg() failed.");

		if (err == EAGAIN) {
			return STU_AGAIN;
		}

		return STU_ERROR;
	}

	if (n == 0) {
		stu_log_debug(0, "recvmsg() returned zero.");
		return STU_ERROR;
	}

	if ((size_t) n < sizeof(stu_filedes_t)) {
		stu_log_error(0, "recvmsg() returned not enough data: %z.", n);
		return STU_ERROR;
	}

	if (fds->command == STU_CMD_OPEN_FILEDES) {
		if (cmsg.cm.cmsg_len < (socklen_t) CMSG_LEN(sizeof(int))) {
			stu_log_error(0, "recvmsg() returned too small ancillary data.");
			return STU_ERROR;
		}

		if (cmsg.cm.cmsg_level != SOL_SOCKET || cmsg.cm.cmsg_type != SCM_RIGHTS) {
			stu_log_error(0, "recvmsg() returned invalid ancillary data level %d or type %d.", cmsg.cm.cmsg_level, cmsg.cm.cmsg_type);
			return STU_ERROR;
		}

		memcpy(&fds->fd, CMSG_DATA(&cmsg.cm), sizeof(int));
	}

	if (msg.msg_flags & (MSG_TRUNC|MSG_CTRUNC)) {
		stu_log_error(0, "recvmsg() truncated data.");
	}

	return n;
}

stu_int_t
stu_filedes_add_event(stu_fd_t fd, uint32_t event, stu_event_handler_pt handler) {
	stu_connection_t  *c;
	stu_event_t       *ev;

	c = stu_connection_get(fd);
	if (c == NULL) {
		return STU_ERROR;
	}

	ev = event == STU_READ_EVENT ? c->read : c->write;
	if (ev == NULL) {
		stu_log_error(0, "Failed to add filedes event: fd=%d, ev=%d.", fd, event);
		return STU_ERROR;
	}
	ev->handler = handler;

	if (stu_epoll_add_event(ev, event) == STU_ERROR) {
		stu_connection_free(c);
		return STU_ERROR;
	}

	return STU_OK;
}

void
stu_filedes_close(stu_fd_t *fds) {
	if (close(fds[0]) == -1) {
		stu_log_error(stu_errno, "close() filedes failed");
	}

	if (close(fds[1]) == -1) {
		stu_log_error(stu_errno, "close() filedes failed");
	}
}

