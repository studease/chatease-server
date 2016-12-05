/*
 * stu_websocket_request.c
 *
 *  Created on: 2016-11-24
 *      Author: Tony Lau
 */

#include <sys/socket.h>
#include "stu_config.h"
#include "stu_core.h"

static stu_int_t stu_websocket_process_request_frames(stu_websocket_request_t *r);
static stu_int_t stu_websocket_send(stu_socket_t fd, stu_buf_t *buf);


void
stu_websocket_wait_request_handler(stu_event_t *rev) {
	stu_connection_t *c;
	stu_int_t         n, err;

	c = (stu_connection_t *) rev->data;

	stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) -1) {
		goto done;
	}

	if (c->buffer.start == NULL) {
		c->buffer.start = (u_char *) stu_base_palloc(c->pool, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE);
		c->buffer.end = c->buffer.start + STU_WEBSOCKET_REQUEST_DEFAULT_SIZE;
	}
	c->buffer.last = c->buffer.start;
	stu_memzero(c->buffer.start, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE);

	n = recv(c->fd, c->buffer.start, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE, 0);
	if (n == -1) {
		err = stu_errno;
		if (err == EAGAIN) {
			goto done;
		}

		stu_log_error(err, "Failed to recv data: fd=%d.", c->fd);
		stu_connection_free(c);
		goto done;
	}

	if (n == 0) {
		stu_log_debug(0, "Remote client has closed connection: fd=%d.", c->fd);
		goto failed;
	}

	stu_log_debug(0, "recv: fd=%d, bytes=%d, str=\n%s", c->fd, n, c->buffer.start);

	c->data = (void *) stu_websocket_create_request(c);
	if (c->data == NULL) {
		stu_log_error(0, "Failed to create websocket request.");
		goto failed;
	}

	stu_websocket_process_request(c->data);

	goto done;

failed:

	c->read->active = FALSE;
	stu_epoll_del_event(c->read, STU_READ_EVENT);

	stu_websocket_close_connection(c);

done:

	stu_spin_unlock(&c->lock);
}

stu_websocket_request_t *
stu_websocket_create_request(stu_connection_t *c) {
	stu_websocket_request_t *r;

	if (c->data == NULL) {
		stu_spin_lock(&c->pool->lock);
		r = stu_base_pcalloc(c->pool, sizeof(stu_websocket_request_t));
		stu_spin_unlock(&c->pool->lock);
	} else {
		r = c->data;
	}

	r->connection = c;
	r->frame_in = &c->buffer;
	r->frame = &r->frames_in;

	return r;
}

void
stu_websocket_process_request(stu_websocket_request_t *r) {
	stu_int_t         rc;
	stu_connection_t *c;
	u_char            raw;
	stu_message_t     msg;
	stu_channel_t     ch;
	stu_userinfo_t   *info;
	stu_error_t       err;

	rc = stu_websocket_process_request_frames(r);
	if (rc == STU_ERROR || rc == STU_AGAIN) {
		return;
	}

	c = r->connection;

	msg.data.start = (u_char *) "hello all!";
	msg.type = (stu_str_t *) &STU_MESSAGE_TYPE_MULTI;
	ch.id = 1;
	ch.state = 3;

	switch (r->command) {
	case STU_WEBSOCKET_CMD_JOIN:
		c->user.id = 1;
		c->user.name.data = stu_base_pcalloc(c->pool, sizeof("Tony") + 1);
		c->user.name.len = 4;
		memcpy(c->user.name.data, "Tony", 4);
		info = stu_base_pcalloc(c->pool, sizeof(stu_userinfo_t));
		if (info == NULL) {
			stu_log_error(0, "Failed to alloc userinfo.");
			return;
		}
		info->id = 1;
		info->role = 8;
		c->user.info = info;
		raw = STU_WEBSOCKET_RAW_IDENTITY;
		break;
	case STU_WEBSOCKET_CMD_LEFT:
		break;
	case STU_WEBSOCKET_CMD_UNI_DATA:
		break;
	case STU_WEBSOCKET_CMD_DATA:
		info = c->user.info;
		raw = STU_WEBSOCKET_RAW_MESSAGE;
		break;

	case STU_WEBSOCKET_CMD_MUTE:
		break;
	case STU_WEBSOCKET_CMD_UNMUTE:
		break;
	case STU_WEBSOCKET_CMD_FORBID:
		break;
	case STU_WEBSOCKET_CMD_RELIVE:
		break;

	case STU_WEBSOCKET_CMD_UPGRADE:
		break;
	case STU_WEBSOCKET_CMD_DEMOTE:
		break;

	case STU_WEBSOCKET_CMD_DISMISS:
		break;
	case STU_WEBSOCKET_CMD_ACTIVE:
		break;
	default:
		err.id = 0xFF;
		err.explain = (stu_str_t *) &STU_ERR_EXP_BAD_REQUEST;
		raw = STU_WEBSOCKET_RAW_ERROR;
		break;
	}

	stu_websocket_finalize_request(r, raw, &msg, &ch, info, &err);
}

static stu_int_t
stu_websocket_process_request_frames(stu_websocket_request_t *r) {
	stu_int_t              rc;
	stu_websocket_frame_t *new;
	stu_connection_t      *c;

	c = r->connection;

	do {
		rc = stu_websocket_parse_frame(r, r->frame_in);
		if (rc == STU_OK) {
			new = stu_base_pcalloc(r->connection->pool, sizeof(stu_websocket_frame_t));
			if (new == NULL) {
				stu_log_error(0, "Failed to alloc new websocket frame.");
				return STU_ERROR;
			}

			r->frame->next = new;
			r->frame = new;
		}
	} while (rc == STU_OK);

	if (rc == STU_DONE) {
		if (c->user.info == NULL) {
			r->command = STU_WEBSOCKET_CMD_JOIN;
		} else {
			r->command = STU_WEBSOCKET_CMD_DATA;
		}
	}

	return rc;
}

void
stu_websocket_finalize_request(stu_websocket_request_t *r, u_char raw, stu_message_t *msg, stu_channel_t *ch, stu_userinfo_t *info, stu_error_t *err) {
	stu_connection_t *c;
	stu_buf_t        *buf;
	stu_str_t        *rd;

	c = r->connection;

	/*c->write->handler = stu_websocket_request_handler;
	if (stu_epoll_add_event(c->write, STU_WRITE_EVENT|EPOLLET) == STU_ERROR) {
		stu_log_error(0, "Failed to add websocket client write event.");
		return;
	}*/

	switch (raw) {
	case STU_WEBSOCKET_RAW_MESSAGE:
		rd = (stu_str_t *) &STU_RAW_DATA_MESSAGE;
		break;
	case STU_WEBSOCKET_RAW_IDENTITY:
		rd = (stu_str_t *) &STU_RAW_DATA_IDENTITY;
		break;
	case STU_WEBSOCKET_RAW_JOIN:
		rd = (stu_str_t *) &STU_RAW_DATA_JOIN;
		break;
	case STU_WEBSOCKET_RAW_LEFT:
		rd = (stu_str_t *) &STU_RAW_DATA_LEFT;
		break;
	case STU_WEBSOCKET_RAW_ERROR:
		rd = (stu_str_t *) &STU_RAW_DATA_ERROR;
		break;
	default:
		break;
	}

	buf = &c->buffer;
	buf->last = buf->start + 4;
	stu_memzero(buf->start, buf->end - buf->start);

	*buf->last++ = '{';
	buf->last = stu_sprintf(buf->last, (const char *) STU_MESSAGE_TEMPLATE_RAW.data, rd->data);
	if (raw == STU_WEBSOCKET_RAW_MESSAGE && msg) {
		*buf->last++ = ',';
		buf->last = stu_sprintf(buf->last, (const char *) STU_MESSAGE_TEMPLATE_DATA.data, msg->data.start, msg->type->data);
	}
	if (ch) {
		*buf->last++ = ',';
		buf->last = stu_sprintf(buf->last, (const char *) STU_MESSAGE_TEMPLATE_CHANNEL.data, ch->id, ch->state, info->role);
		*buf->last++ = ',';
		buf->last = stu_sprintf(buf->last, (const char *) STU_MESSAGE_TEMPLATE_USER.data, c->user.id, c->user.name.data);
	}
	if (raw == STU_WEBSOCKET_RAW_ERROR && err) {
		*buf->last++ = ',';
		buf->last = stu_sprintf(buf->last, (const char *) STU_MESSAGE_TEMPLATE_ERROR.data, err->id, err->explain->data);
	}
	*buf->last++ = '}';

	stu_websocket_send(c->fd, buf);
}


void
stu_websocket_request_handler(stu_event_t *wev) {
	stu_websocket_request_t *r;
	stu_connection_t        *c;
	stu_buf_t               *buf;
	stu_int_t                n;

	c = (stu_connection_t *) wev->data;

	stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) -1) {
		goto done;
	}

	//stu_epoll_del_event(c->write, STU_WRITE_EVENT);

	buf = &c->buffer;
	buf->last = buf->start;
	stu_memzero(buf->start, buf->end - buf->start);

	r = (stu_websocket_request_t *) c->data;
	if (r == NULL) {
		goto failed;
	}

	*buf->last++ = 129;
	*buf->last++ = 88;
	buf->last = stu_memcpy(buf->last, "{\"raw\":\"identity\",\"channel\":{\"id\":\"1\",\"state\":3,\"role\":0},\"user\":{\"id\":1,\"name\":\"Tony\"}}", 88);

	n = send(c->fd, buf->start, stu_strlen(buf->start), 0);
	if (n == -1) {
		stu_log_debug(0, "Failed to send data: fd=%d.", c->fd);
		goto failed;
	}

	stu_log_debug(0, "sent: fd=%d, bytes=%d, str=\n%s", c->fd, n, buf->start);

	goto done;

failed:

	c->read->active = FALSE;
	stu_epoll_del_event(c->read, STU_READ_EVENT);

	stu_http_close_connection(c);

done:

	stu_spin_unlock(&c->lock);
}


static stu_int_t
stu_websocket_send(stu_socket_t fd, stu_buf_t *buf) {
	size_t            size;
	u_char           *data;
	uint16_t         *s16;
	stu_int_t         n;

	size = buf->last - buf->start - 4;
	data = buf->start;
	if (size < 126) {
		data += 2;
		data[0] = 0x80 | STU_WEBSOCKET_OPCODE_TEXT;
		data[1] = size;
	} else if (size < 65536) {
		data[0] = 0x80 | STU_WEBSOCKET_OPCODE_TEXT;
		data[1] = 0x7E;
		s16 = (uint16_t *) &buf->start[2];
		*s16 = size;
	} else {
		stu_log_error(0, "message too long.");
		return STU_ERROR;
	}

	n = send(fd, data, size + (size < 126 ? 2 : 4), 0);
	if (n == -1) {
		stu_log_debug(0, "Failed to send data: fd=%d.", fd);
		return STU_ERROR;
	}

	stu_log_debug(0, "s[0]=%d, s[1]=%d, s[2]=%d, s[3]=%d", buf->start[0], buf->start[1], buf->start[2], buf->start[3]);
	stu_log_debug(0, "sent: fd=%d, bytes=%d, str=\n%s", fd, n, data + (size < 126 ? 2 : 4));

	return STU_OK;
}


void
stu_websocket_close_request(stu_websocket_request_t *r, stu_int_t rc) {
	stu_connection_t *c;

	c = r->connection;

	stu_websocket_free_request(r, rc);
	stu_websocket_close_connection(c);
}

void
stu_websocket_free_request(stu_websocket_request_t *r, stu_int_t rc) {
	r->connection->data = NULL;
}

void
stu_websocket_close_connection(stu_connection_t *c) {
	stu_connection_close(c);
}


void
stu_websocket_empty_handler(stu_event_t *wev) {

}

void
stu_websocket_request_empty_handler(stu_websocket_request_t *r) {

}

