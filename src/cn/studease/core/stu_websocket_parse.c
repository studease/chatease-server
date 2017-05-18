/*
 * stu_websocket_parse.c
 *
 *  Created on: 2016-12-5
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_int_t
stu_websocket_parse_frame(stu_websocket_request_t *r, stu_buf_t *b) {
	u_char    *p;
	stu_buf_t *buf;
	uint64_t   i;
	enum {
		sw_fin = 0,
		sw_mask,
		sw_extended_2,
		sw_extended_8,
		sw_masking_key,
		sw_payload_data
	} state;

	buf = &r->frame->payload_data;
	state = r->state;

	for (p = b->last; p < b->end; p++) {
		switch (state) {
		case sw_fin:
			r->frame->fin = (*p >> 7) & 0x1;
			r->frame->rsv1 = (*p >> 6) & 0x1;
			r->frame->rsv2 = (*p >> 5) & 0x1;
			r->frame->rsv3 = (*p >> 4) & 0x1;
			r->frame->opcode = *p & 0xF;
			state = sw_mask;
			break;
		case sw_mask:
			r->frame->mask = (*p >> 7) & 0x1;
			r->frame->payload_len = *p & 0x7F;
			if (r->frame->payload_len == 126) {
				state = sw_extended_2;
			} else if (r->frame->payload_len == 127) {
				state = sw_extended_8;
			} else {
				r->frame->extended = r->frame->payload_len;
				state = r->frame->mask ? sw_masking_key : sw_payload_data;
			}
			break;
		case sw_extended_2:
			r->frame->extended =  *p++ << 8;
			r->frame->extended |= *p;
			state = r->frame->mask ? sw_masking_key : sw_payload_data;
			break;
		case sw_extended_8:
			r->frame->extended =  (i = *p++) << 56;
			r->frame->extended |= (i = *p++) << 48;
			r->frame->extended |= (i = *p++) << 40;
			r->frame->extended |= (i = *p++) << 32;
			r->frame->extended |= (i = *p++) << 24;
			r->frame->extended |= (i = *p++) << 16;
			r->frame->extended |= (i = *p++) << 8;
			r->frame->extended |= *p;
			state = r->frame->mask ? sw_masking_key : sw_payload_data;
			break;
		case sw_masking_key:
			memcpy(r->frames_in.masking_key, p, 4);
			p += 3;
			state = sw_payload_data;
			break;
		case sw_payload_data:
			switch (r->frame->opcode) {
			case STU_WEBSOCKET_OPCODE_TEXT:
			case STU_WEBSOCKET_OPCODE_BINARY:
				if (r->frame->mask) {
					for (i = 0; i < r->frame->extended; i++) {
						p[i] ^= r->frame->masking_key[i % 4];
					}
					stu_log_debug(0, "unmasked: %s", p);
				}
				break;
			case STU_WEBSOCKET_OPCODE_CLOSE:
				stu_log_debug(0, "close frame.");
				break;
			case STU_WEBSOCKET_OPCODE_PING:
				stu_log_debug(0, "ping frame.");
				break;
			case STU_WEBSOCKET_OPCODE_PONG:
				stu_log_debug(0, "pong frame.");
				break;
			default:
				break;
			}

			buf->start = buf->last = p;
			if (b->end - p >= r->frame->extended) {
				buf->end = p + r->frame->extended;
				p = buf->end;
				if (r->frame->fin) {
					goto frame_done;
				}
				goto done;
			}
			goto again;
			break;
		}
	}

again:

	b->last = p;
	r->state = state;

	return STU_AGAIN;

done:

	b->last = p;
	r->state = sw_fin;

	return STU_OK;

frame_done:

	b->last = p;
	r->state = sw_fin;

	return STU_DONE;
}


