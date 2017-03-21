/*
 * stu_websocket_request.c
 *
 *  Created on: 2016-11-24
 *      Author: Tony Lau
 */

#include <sys/socket.h>
#include <sys/time.h>
#include "stu_config.h"
#include "stu_core.h"

static stu_int_t stu_websocket_process_request_frames(stu_websocket_request_t *r);
static void stu_websocket_analyze_request(stu_websocket_request_t *r, u_char *text, size_t size);


void
stu_websocket_wait_request_handler(stu_event_t *rev) {
	stu_connection_t *c;
	stu_channel_t    *ch;
	stu_int_t         n, err;

	c = (stu_connection_t *) rev->data;

	stu_spin_lock(&c->lock);
	if (c->fd == (stu_socket_t) -1) {
		stu_log_error(0, "websocket waited a invalid fd=%d.", c->fd);
		goto done;
	}

	if (c->buffer.start == NULL) {
		c->buffer.start = (u_char *) stu_base_palloc(c->pool, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE);
		c->buffer.end = c->buffer.start + STU_WEBSOCKET_REQUEST_DEFAULT_SIZE;
	}
	c->buffer.last = c->buffer.start;
	stu_memzero(c->buffer.start, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE);

again:

	n = recv(c->fd, c->buffer.start, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE, 0);
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

	stu_log_debug(4, "recv: fd=%d, bytes=%d.", c->fd, n);

	c->data = (void *) stu_websocket_create_request(c);
	if (c->data == NULL) {
		stu_log_error(0, "Failed to create websocket request.");
		goto failed;
	}

	stu_websocket_process_request(c->data);

	goto done;

failed:

	c->read.active = FALSE;
	stu_epoll_del_event(&c->read, STU_READ_EVENT);

	ch = c->user.channel;
	stu_channel_remove(ch, c);
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
	stu_websocket_frame_t *f;
	stu_connection_t      *c;
	stu_int_t              rc;
	stu_buf_t              buf;
	u_char                 temp[STU_WEBSOCKET_REQUEST_DEFAULT_SIZE];
	uint64_t               len, size;

	c = r->connection;
	if (c->user.channel == NULL) {
		stu_log_error(0, "Failed to process request: channel not found.");
		stu_websocket_finalize_request(r, STU_HTTP_UNAUTHORIZED);
		return;
	}

	rc = stu_websocket_process_request_frames(r);
	if (rc != STU_DONE) {
		stu_log_error(0, "Failed to process request frames.");
		stu_websocket_finalize_request(r, STU_HTTP_BAD_REQUEST);
		return;
	}

	// concat keyframe string
	stu_memzero(temp, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE);

	buf.start = buf.last = temp;
	buf.end = buf.start + STU_WEBSOCKET_REQUEST_DEFAULT_SIZE;

	for (f = &r->frames_in; f; f = f->next) {
		if (f->opcode != STU_WEBSOCKET_OPCODE_TEXT) {
			continue;
		}

		len = f->payload_data.end - f->payload_data.start;
		if (len == 0) {
			continue;
		}

		buf.last = stu_memcpy(buf.last, f->payload_data.start, len);

		f->payload_data.end = f->payload_data.start;
	}

	size = buf.last - buf.start;
	if (size == 0) {
		stu_log_error(0, "Failed to concat keyframe string: size=0.");
		return;
	}

	stu_websocket_analyze_request(r, (u_char *) temp, size);
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

static void
stu_websocket_analyze_request(stu_websocket_request_t *r, u_char *text, size_t size) {
	stu_connection_t      *c;
	stu_json_t            *req, *cmd, *rqdata, *rqtype, *rqchannel;
	stu_json_t            *res, *raw, *rsdata, *rstype, *rschannel, *rsuser, *rsuid, *rsuname, *rsurole;
	stu_str_t             *str;
	stu_websocket_frame_t *out;
	u_char                *data, temp[STU_WEBSOCKET_REQUEST_DEFAULT_SIZE];

	c = r->connection;

	req = stu_json_parse((u_char *) text, size);
	if (req == NULL || req->type != STU_JSON_TYPE_OBJECT) {
		stu_log_error(0, "Failed to parse request.");
		stu_websocket_finalize_request(r, STU_HTTP_BAD_REQUEST);
		return;
	}

	cmd = stu_json_get_object_item_by(req, &STU_PROTOCOL_CMD);
	if (cmd == NULL || cmd->type != STU_JSON_TYPE_STRING) {
		stu_log_error(0, "Failed to analyze request: \"cmd\" not found.");
		stu_json_delete(req);
		stu_websocket_finalize_request(r, STU_HTTP_BAD_REQUEST);
		return;
	}

	str = (stu_str_t *) cmd->value;
	if (stu_strncmp(str->data, STU_PROTOCOL_CMDS_TEXT.data, STU_PROTOCOL_CMDS_TEXT.len) == 0) {
		rqdata = stu_json_get_object_item_by(req, &STU_PROTOCOL_DATA);
		rqtype = stu_json_get_object_item_by(req, &STU_PROTOCOL_TYPE);
		rqchannel = stu_json_get_object_item_by(req, &STU_PROTOCOL_CHANNEL);
		if (rqdata == NULL || rqdata->type != STU_JSON_TYPE_STRING
				|| rqtype == NULL || rqtype->type != STU_JSON_TYPE_STRING
				|| rqchannel == NULL || rqchannel->type != STU_JSON_TYPE_OBJECT) {
			stu_log_error(0, "Failed to analyze request: necessary param[s] not found.");
			stu_json_delete(req);
			stu_websocket_finalize_request(r, STU_HTTP_BAD_REQUEST);
			return;
		}

		res = stu_json_create_object(NULL);
		raw = stu_json_create_string(&STU_PROTOCOL_RAW, STU_PROTOCOL_RAWS_TEXT.data, STU_PROTOCOL_RAWS_TEXT.len);
		rsdata = stu_json_duplicate(rqdata, FALSE);
		rstype = stu_json_duplicate(rqtype, FALSE);
		rschannel = stu_json_duplicate(rqchannel, TRUE);
		rsuser = stu_json_create_object(&STU_PROTOCOL_USER);

		rsuid = stu_json_create_string(&STU_PROTOCOL_ID, c->user.strid.data, c->user.strid.len);
		rsuname = stu_json_create_string(&STU_PROTOCOL_NAME, c->user.name.data, c->user.name.len);
		rsurole = stu_json_create_number(&STU_PROTOCOL_ROLE, (stu_double_t) c->user.role);

		stu_json_add_item_to_object(rsuser, rsuid);
		stu_json_add_item_to_object(rsuser, rsuname);
		stu_json_add_item_to_object(rsuser, rsurole);

		stu_json_add_item_to_object(res, raw);
		stu_json_add_item_to_object(res, rsdata);
		stu_json_add_item_to_object(res, rstype);
		stu_json_add_item_to_object(res, rschannel);
		stu_json_add_item_to_object(res, rsuser);

		stu_memzero(temp, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE);
		data = stu_json_stringify(res, (u_char *) temp);

		stu_json_delete(req);
		stu_json_delete(res);

		// setup out frame.
		out = &r->frames_out;
		out->opcode = STU_WEBSOCKET_OPCODE_TEXT;
		out->extended = data - temp;
		out->payload_data.start = temp;
		out->payload_data.end = out->payload_data.last = data;

		stu_websocket_finalize_request(r, STU_HTTP_OK);

		return;
	}

	stu_json_delete(req);

	stu_websocket_finalize_request(r, STU_HTTP_METHOD_NOT_ALLOWED);
}

void
stu_websocket_finalize_request(stu_websocket_request_t *r, stu_int_t rc) {
	stu_connection_t *c;

	r->status = rc;
	c = r->connection;

	/*c->write->handler = stu_websocket_request_handler;
	if (stu_epoll_add_event(c->write, STU_WRITE_EVENT|EPOLLET) == STU_ERROR) {
		stu_log_error(0, "Failed to add websocket client write event.");
		return;
	}*/

	stu_websocket_request_handler(&c->write);
}

void
stu_websocket_request_handler(stu_event_t *wev) {
	stu_websocket_request_t *r;
	stu_connection_t        *c, *t;
	stu_channel_t           *ch;
	stu_websocket_frame_t   *f;
	stu_buf_t                buf;
	u_char                   temp[STU_WEBSOCKET_REQUEST_DEFAULT_SIZE], *data;
	stu_int_t                extened, n;
	uint64_t                 size;
	stu_list_elt_t          *elts;
	stu_hash_elt_t          *e;
	stu_queue_t             *q;
	struct timeval           start, end;
	stu_int_t                cost;

	c = (stu_connection_t *) wev->data;
	r = (stu_websocket_request_t *) c->data;
	ch = c->user.channel;

	stu_memzero(temp, STU_WEBSOCKET_REQUEST_DEFAULT_SIZE);

	buf.start = temp;
	buf.end = buf.start + STU_WEBSOCKET_REQUEST_DEFAULT_SIZE;

	for (f = &r->frames_out; f; f = f->next) {
		buf.last = buf.start + 10;
		buf.last = stu_memcpy(buf.last, f->payload_data.start, f->extended);

		data = buf.start;
		size = f->extended;
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

		stu_log_debug(4, "frame header: %d %d %d %d %d %d %d %d %d %d",
				buf.start[0], buf.start[1], buf.start[2], buf.start[3], buf.start[4],
				buf.start[5], buf.start[6], buf.start[7], buf.start[8], buf.start[9]);

		gettimeofday(&start, NULL);

		if (r->status == STU_HTTP_OK) {
			stu_spin_lock(&ch->userlist.lock);

			elts = &ch->userlist.keys.elts;
			for (q = stu_queue_head(&elts->queue); q != stu_queue_sentinel(&elts->queue); q = stu_queue_next(q)) {
				e = stu_queue_data(q, stu_hash_elt_t, q);
				t = (stu_connection_t *) e->value;

				n = send(t->fd, data, size + 2 + extened, 0);
				if (n == -1) {
					stu_log_error(stu_errno, "Failed to send data: from=%d, to=%d.", c->fd, t->fd);
					/*
					q = stu_queue_prev(q);

					stu_channel_remove_locked(ch, t);
					stu_websocket_close_connection(t);
					*/
					continue;
				}

				//stu_log_debug(4, "sent to fd=%d, bytes=%d.", t->fd, n);
			}

			stu_spin_unlock(&ch->userlist.lock);
		} else {
			n = send(c->fd, data, size + 2 + extened, 0);
			if (n == -1) {
				stu_log_error(stu_errno, "Failed to send data: to=%d.", c->fd);
				/*
				stu_channel_remove(ch, c);
				stu_websocket_close_connection(c);
				*/
			}
		}

		gettimeofday(&end, NULL);
		cost = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
		stu_log_debug(4, "sent: fd=%d, bytes=%d, cost=%ldms.", c->fd, n, cost);
	}
}


void
stu_websocket_close_request(stu_websocket_request_t *r, stu_int_t rc) {
	stu_connection_t *c;
	stu_channel_t    *ch;

	stu_websocket_free_request(r, rc);

	c = r->connection;
	ch = c->user.channel;

	stu_channel_remove(ch, c);
	stu_websocket_close_connection(c);
}

void
stu_websocket_free_request(stu_websocket_request_t *r, stu_int_t rc) {
	r->connection->data = NULL;
}

void
stu_websocket_close_connection(stu_connection_t *c) {
	stu_http_close_connection(c);
}


void
stu_websocket_empty_handler(stu_event_t *wev) {

}

void
stu_websocket_request_empty_handler(stu_websocket_request_t *r) {

}

