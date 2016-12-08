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


void
stu_websocket_wait_request_handler(stu_event_t *rev) {
	stu_connection_t *c;
	stu_channel_t    *ch;
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
			stu_log_error(err, "Failed to recv data: fd=%d.", c->fd);
			goto done;
		}

		stu_log_error(err, "Failed to recv data: fd=%d.", c->fd);
		goto failed;
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

	ch = c->user.channel;
	stu_spin_lock(&ch->userlist.lock);
	stu_websocket_close_connection(c);
	stu_spin_lock(&ch->userlist.lock);

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
	stu_channel_t    *ch;

	rc = stu_websocket_process_request_frames(r);
	if (rc != STU_DONE) {
		return;
	}

	c = r->connection;
	ch = c->user.channel;

	if (ch == NULL) {
		stu_log_error(0, "Failed to process request: %d, channel not found.", STU_HTTP_INTERNAL_SERVER_ERROR);

		stu_spin_lock(&ch->userlist.lock);
		stu_websocket_close_request(r, STU_HTTP_INTERNAL_SERVER_ERROR);
		stu_spin_lock(&ch->userlist.lock);

		return;
	}

	stu_websocket_finalize_request(r, ch);
}

static stu_int_t
stu_websocket_process_request_frames(stu_websocket_request_t *r) {
	stu_int_t              rc;
	stu_websocket_frame_t *new;

	do {
		rc = stu_websocket_parse_frame(r, r->frame_in);
		if (rc == STU_OK) {
			if (r->frame->next == NULL) {
				new = stu_base_pcalloc(r->connection->pool, sizeof(stu_websocket_frame_t));
				if (new == NULL) {
					stu_log_error(0, "Failed to alloc new websocket frame.");
					return STU_ERROR;
				}

				r->frame->next = new;
			} else {
				new  = r->frame->next;
			}

			r->frame = new;
		}
	} while (rc == STU_OK);

	return rc;
}

void
stu_websocket_finalize_request(stu_websocket_request_t *r, stu_channel_t *ch) {
	stu_connection_t      *c, *t;
	stu_websocket_frame_t *f;
	stu_buf_t              buf;
	u_char                 temp[STU_WEBSOCKET_REQUEST_DEFAULT_SIZE], *data;
	stu_int_t              s, extened, n;
	uint64_t               size;
	stu_list_elt_t        *elts;
	stu_hash_elt_t        *e;
	stu_queue_t           *q;

	c = r->connection;

	/*c->write->handler = stu_websocket_request_handler;
	if (stu_epoll_add_event(c->write, STU_WRITE_EVENT|EPOLLET) == STU_ERROR) {
		stu_log_error(0, "Failed to add websocket client write event.");
		return;
	}*/

	stu_memzero(temp, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE);

	buf.start = temp;
	buf.last = buf.start + 10;

	for (f = &r->frames_in; f; f = f->next) {
		if (f->opcode != STU_WEBSOCKET_OPCODE_TEXT) {
			continue;
		}

		s = f->payload_data.end - f->payload_data.start;
		if (s == 0) {
			continue;
		}

		buf.last = stu_memcpy(buf.last, f->payload_data.start, s);

		f->payload_data.end = f->payload_data.start;
	}

	size = buf.last - buf.start - 10;
	if (size == 0) {
		return;
	}

	data = buf.start;
	if (size < 126) {
		data += 8;
		data[0] = 0x80 | STU_WEBSOCKET_OPCODE_TEXT;
		data[1] = size;
		extened = 0;
	} else if (size < 65536) {
		data += 6;
		data[0] = 0x80 | STU_WEBSOCKET_OPCODE_TEXT;
		data[1] = 0x7E;
		data[2] = size >> 8;
		data[3] = size;
		extened = 2;
	} else {
		data[0] = 0x80 | STU_WEBSOCKET_OPCODE_TEXT;
		data[1] = 0x7F;
		data[2] = size >> 56;
		data[3] = size >> 48;
		data[4] = size >> 40;
		data[5] = size >> 32;
		data[6] = size >> 24;
		data[7] = size >> 16;
		data[8] = size >> 8;
		data[9] = size;
		extened = 8;
	}

	stu_log_debug(0, "frame header: %d %d %d %d %d %d %d %d %d %d",
			buf.start[0], buf.start[1], buf.start[2], buf.start[3], buf.start[4],
			buf.start[5], buf.start[6], buf.start[7], buf.start[8], buf.start[9]);

	stu_spin_lock(&ch->userlist.lock);
	elts = &ch->userlist.keys.elts;
	for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
		e = stu_queue_data(q, stu_hash_elt_t, q);
		t = (stu_connection_t *) e->value;

		n = send(t->fd, data, size + 2 + extened, 0);
		if (n == -1) {
			stu_log_error(0, "Failed to send data: from=%d, to=%d.", c->fd, t->fd);
			stu_websocket_close_connection(t);
			continue;
		}

		stu_log_debug(0, "sent to fd=%d.", t->fd);
	}
	stu_spin_unlock(&ch->userlist.lock);

	stu_log_debug(0, "sent: fd=%d, bytes=%d, str=\n%s", c->fd, n, buf.start + 10);
}


void
stu_websocket_request_handler(stu_event_t *wev) {
	stu_websocket_request_t *r;
	stu_connection_t        *c;
	stu_channel_t           *ch;
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

	ch = c->user.channel;
	stu_spin_lock(&ch->userlist.lock);
	stu_websocket_close_connection(c);
	stu_spin_lock(&ch->userlist.lock);

done:

	stu_spin_unlock(&c->lock);
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
	stu_channel_t *ch;
	stu_uint_t     kh;
	stu_str_t     *key;

	ch = c->user.channel;

	key = &c->user.strid;
	kh = stu_hash_key_lc(key->data, key->len);
	stu_hash_remove(&ch->userlist, kh, key->data, key->len);
	stu_log_debug(0, "removed user(\"%s\"), total=%lu.", key->data, ch->userlist.length);

	stu_connection_close(c);
}


void
stu_websocket_empty_handler(stu_event_t *wev) {

}

void
stu_websocket_request_empty_handler(stu_websocket_request_t *r) {

}

