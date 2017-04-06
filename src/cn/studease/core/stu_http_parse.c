/*
 * stu_http_parse.c
 *
 *  Created on: 2016-11-28
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"


stu_int_t
stu_http_parse_request_line(stu_http_request_t *r, stu_buf_t *b) {
	u_char  ch, *p, *s, *v;
	enum {
		sw_start = 0,
		sw_method,
		sw_spaces_before_uri,
		sw_uri,
		sw_args,
		sw_spaces_before_ver,
		sw_version,
		sw_almost_done
	} state;

	state = r->state;

	for (p = b->last; p < b->end; p++) {
		ch = *p;

		switch (state) {
		case sw_start:
			r->request_line.data = p;
			state = sw_method;
			break;
		case sw_method:
			if (ch == ' ') {
				s = r->request_line.data;

				switch (p - s) {
				case 3:
					if (stu_strncmp(s, "GET", 3) == 0) {
						r->method = STU_HTTP_GET;
						break;
					}
					break;
				case 4:
					if (stu_strncmp(s, "POST", 4) == 0) {
						r->method = STU_HTTP_POST;
						break;
					}
					break;
				}

				state = sw_spaces_before_uri;
				break;
			}
			if (ch < 'A' || ch > 'Z') {
				return STU_ERROR;
			}
			break;
		case sw_spaces_before_uri:
			r->uri.data = r->target.data = p;
			state = sw_uri;
			break;
		case sw_uri:
			if (ch == '?') {
				r->uri.len = p - r->uri.data;

				if (*r->target.data == '/') {
					r->target.data++;
				}
				r->target.len = p - r->target.data;

				r->args.data = p + 1;
				state = sw_args;
				break;
			}
			if (ch == ' ') {
				r->uri.len = p - r->uri.data;

				if (*r->target.data == '/') {
					r->target.data++;
				}
				r->target.len = p - r->target.data;

				state = sw_spaces_before_ver;
				break;
			}
			if (ch == '/') {
				r->target.data = p;
			}
			break;
		case sw_args:
			if (ch == ' ') {
				r->args.len = p - r->args.data;
				state = sw_spaces_before_ver;
				break;
			}
			break;
		case sw_spaces_before_ver:
			v = p;
			state = sw_version;
			break;
		case sw_version:
			if (ch == CR) {
				if (stu_strncmp(v, "HTTP/1.1", 8) == 0) {
					r->http_version = STU_HTTP_VERSION_11;
					r->request_line.len = p - r->request_line.data;
					state = sw_almost_done;
					break;
				}
				if (stu_strncmp(v, "HTTP/1.0", 8) == 0) {
					r->http_version = STU_HTTP_VERSION_10;
					r->request_line.len = p - r->request_line.data;
					state = sw_almost_done;
					break;
				}
				return STU_ERROR;
			}
			break;
		case sw_almost_done:
			switch (ch) {
			case LF:
				goto done;
			default:
				return STU_ERROR;
			}
			break;
		}
	}

	b->last = p;
	r->state = state;

	return STU_AGAIN;

done:

	b->last = p + 1;
	r->state = sw_start;

	return STU_OK;
}

stu_int_t
stu_http_parse_status_line(stu_http_request_t *r, stu_buf_t *b) {
	u_char  ch, *p, *s, *v;
	enum {
		sw_start = 0,
		sw_version,
		sw_spaces_before_status,
		sw_status,
		sw_spaces_before_explain,
		sw_explain,
		sw_almost_done
	} state;

	state = r->state;

	for (p = b->last; p < b->end; p++) {
		ch = *p;

		switch (state) {
		case sw_start:
			r->headers_out.status_line.data = v = p;
			state = sw_version;
			break;
		case sw_version:
			if (ch == ' ') {
				if (stu_strncmp(v, "HTTP/1.1", 8) == 0) {
					r->http_version = STU_HTTP_VERSION_11;
					state = sw_spaces_before_status;
					break;
				}
				if (stu_strncmp(v, "HTTP/1.0", 8) == 0) {
					r->http_version = STU_HTTP_VERSION_10;
					state = sw_spaces_before_status;
					break;
				}
				return STU_ERROR;
				break;
			}
			break;
		case sw_spaces_before_status:
			s = p;
			state = sw_status;
			break;
		case sw_status:
			if (ch == ' ') {
				r->headers_out.status = atol((const char *) s);
				state = sw_spaces_before_explain;
				break;
			}
			if (p - s >= 3) {
				return STU_ERROR;
				break;
			}
			break;
		case sw_spaces_before_explain:
			state = sw_explain;
			break;
		case sw_explain:
			if (ch == CR) {
				state = sw_almost_done;
				break;
			}
			break;
		case sw_almost_done:
			switch (ch) {
			case LF:
				goto done;
			default:
				return STU_ERROR;
			}
			break;
		}
	}

	b->last = p;
	r->state = state;

	return STU_AGAIN;

done:

	b->last = p + 1;
	r->state = sw_start;

	return STU_OK;
}

stu_int_t
stu_http_parse_header_line(stu_http_request_t *r, stu_buf_t *b, stu_uint_t allow_underscores) {
	u_char      c, ch, *p;
	stu_uint_t  hash, i;
	enum {
		sw_start = 0,
		sw_name,
		sw_space_before_value,
		sw_value,
		sw_space_after_value,
		sw_ignore_line,
		sw_almost_done,
		sw_header_almost_done
	} state;

	/* the last '\0' is not needed because string is zero terminated */
	static u_char  lowcase[] =
			"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
			"\0\0\0\0\0\0\0\0\0\0\0\0\0-\0\0" "0123456789\0\0\0\0\0\0"
			"\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
			"\0abcdefghijklmnopqrstuvwxyz\0\0\0\0\0"
			"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
			"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
			"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
			"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";

	state = r->state;
	hash = r->header_hash;
	i = r->lowcase_index;

	for (p = b->last; p < b->end; p++) {
		ch = *p;

		switch (state) {

		/* first char */
		case sw_start:
			r->header_name_start = p;
			r->invalid_header = FALSE;

			switch (ch) {
			case CR:
				r->header_end = p;
				state = sw_header_almost_done;
				break;
			case LF:
				r->header_end = p;
				goto header_done;
			default:
				state = sw_name;

				c = lowcase[ch];
				if (c) {
					hash = stu_hash(0, c);
					r->lowcase_header[0] = c;
					i = 1;
					break;
				}

				if (ch == '_') {
					if (allow_underscores) {
						hash = stu_hash(0, ch);
						r->lowcase_header[0] = ch;
						i = 1;
					} else {
						r->invalid_header = TRUE;
					}
					break;
				}

				if (ch == '\0') {
					return STU_ERROR;
				}

				r->invalid_header = TRUE;
				break;
			}
			break;

		/* header name */
		case sw_name:
			c = lowcase[ch];
			if (c) {
				hash = stu_hash(hash, c);
				r->lowcase_header[i++] = c;
				i &= (STU_HTTP_LC_HEADER_LEN - 1);
				break;
			}

			if (ch == '_') {
				if (allow_underscores) {
					hash = stu_hash(hash, ch);
					r->lowcase_header[i++] = ch;
					i &= (STU_HTTP_LC_HEADER_LEN - 1);
				} else {
					r->invalid_header = TRUE;
				}
				break;
			}

			if (ch == ':') {
				r->header_name_end = p;
				state = sw_space_before_value;
				break;
			}

			if (ch == CR) {
				r->header_name_end = p;
				r->header_start = p;
				r->header_end = p;
				state = sw_almost_done;
				break;
			}

			if (ch == LF) {
				r->header_name_end = p;
				r->header_start = p;
				r->header_end = p;
				goto done;
			}

			/* IIS may send the duplicate "HTTP/1.1 ..." lines */
			if (ch == '/'
					&& p - r->header_name_start == 4
					&& stu_strncmp(r->header_name_start, "HTTP", 4) == 0) {
				state = sw_ignore_line;
				break;
			}

			if (ch == '\0') {
				return STU_ERROR;
			}

			r->invalid_header = TRUE;
			break;

		/* space* before header value */
		case sw_space_before_value:
			switch (ch) {
			case ' ':
				break;
			case CR:
				r->header_start = p;
				r->header_end = p;
				state = sw_almost_done;
				break;
			case LF:
				r->header_start = p;
				r->header_end = p;
				goto done;
			case '\0':
				return STU_ERROR;
			default:
				r->header_start = p;
				state = sw_value;
				break;
			}
			break;

		/* header value */
		case sw_value:
			switch (ch) {
			case ' ':
				r->header_end = p;
				state = sw_space_after_value;
				break;
			case CR:
				r->header_end = p;
				state = sw_almost_done;
				break;
			case LF:
				r->header_end = p;
				goto done;
			case '\0':
				return STU_ERROR;
			}
			break;

		/* space* before end of header line */
		case sw_space_after_value:
			switch (ch) {
			case ' ':
				break;
			case CR:
				state = sw_almost_done;
				break;
			case LF:
				goto done;
			case '\0':
				return STU_ERROR;
			default:
				state = sw_value;
				break;
			}
			break;

		/* ignore header line */
		case sw_ignore_line:
			switch (ch) {
			case LF:
				state = sw_start;
				break;
			default:
				break;
			}
			break;

		/* end of header line */
		case sw_almost_done:
			switch (ch) {
			case LF:
				goto done;
			case CR:
				break;
			default:
				return STU_ERROR;
			}
			break;

		/* end of header */
		case sw_header_almost_done:
			switch (ch) {
			case LF:
				goto header_done;
			default:
				return STU_ERROR;
			}
			break;
		}
	}

	b->last = p;
	r->state = state;
	r->header_hash = hash;
	r->lowcase_index = i;

	return STU_AGAIN;

done:

	b->last = p + 1;
	r->state = sw_start;
	r->header_hash = hash;
	r->lowcase_index = i;

	return STU_OK;

header_done:

	b->last = p + 1;
	r->state = sw_start;

	return STU_DONE;
}


stu_int_t
stu_http_parse_uri(stu_http_request_t *r) {
	return STU_OK;
}


void
stu_http_split_args(stu_http_request_t *r, stu_str_t *uri, stu_str_t *args) {

}

stu_int_t
stu_http_arg(stu_http_request_t *r, u_char *name, size_t len, stu_str_t *value) {
	u_char  *p, *last;

	if (r->args.len == 0) {
		return STU_DECLINED;
	}

	last = r->args.data + r->args.len;
	for (p = r->args.data; p < last; p++) {
		p = stu_strnstr(p, (char *) name, len - 1);
		if (p == NULL) {
			return STU_DECLINED;
		}

		if ((p == r->args.data || *(p - 1) == '&') && *(p + len) == '=') {
			value->data = p + len + 1;

			p = stu_strlchr(p, last, '&');
			if (p == NULL) {
				p = r->args.data + r->args.len;
			}

			value->len = p - value->data;

			return STU_OK;
		}
	}

	return STU_DECLINED;
}

