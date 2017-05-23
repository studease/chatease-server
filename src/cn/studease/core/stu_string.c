/*
 * stu_string.c
 *
 *  Created on: 2016-9-8
 *      Author: Tonu Lau
 */

#include <stdarg.h>
#include "stu_config.h"
#include "stu_core.h"

void
stu_strlow(u_char *dst, u_char *src, size_t n) {
	while (n) {
		*dst = stu_tolower(*src);
		dst++;
		src++;
		n--;
	}
}

u_char *
stu_strlchr(u_char *p, u_char *last, u_char c) {
	while (p < last) {
		if (*p == c) {
			return p;
		}
		p++;
	}
	return NULL;
}

u_char *
stu_strrchr(u_char *p, u_char *last, u_char c) {
	while (p < last--) {
		if (*last == c) {
			return last;
		}
	}
	return NULL;
}

void *
stu_memzero(void *block, size_t n) {
	return memset(block, 0, n);
}

u_char *
stu_strncpy(u_char *dst, u_char *src, size_t n) {
	if (n == 0) {
		return dst;
	}

	while (n--) {
		*dst = *src;

		if (*dst == '\0') {
			return dst;
		}

		dst++;
		src++;
	}

	*dst = '\0';

	return dst;
}

u_char *
stu_strnstr(u_char *s1, char *s2, size_t n) {
	u_char  c1, c2;

	c2 = *(u_char *) s2++;

	do {
		do {
			c1 = *s1++;

			if (c1 == 0) {
				return NULL;
			}
		} while (c1 != c2);
	} while (stu_strncmp(s1, (u_char *) s2, n) != 0);

	return --s1;
}

stu_int_t
stu_strncasecmp(u_char *s1, u_char *s2, size_t n) {
	stu_uint_t  c1, c2;

	while (n) {
		c1 = (stu_uint_t) *s1++;
		c2 = (stu_uint_t) *s2++;

		c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
		c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;

		if (c1 == c2) {
			if (c1) {
				n--;
				continue;
			}

			return 0;
		}

		return c1 - c2;
	}

	return 0;
}


stu_int_t
stu_printf(const char *fmt, ...) {
	va_list    args;
	stu_int_t  n;

	va_start(args, fmt);
	n = stu_vprintf(fmt, args);
	va_end(args);

	return n;
}

u_char *
stu_sprintf(u_char *s, const char *fmt, ...) {
	va_list    args;
	u_char    *p;

	va_start(args, fmt);
	p = stu_vsprintf(s, fmt, args);
	va_end(args);

	return p;
}

stu_int_t
stu_vprintf(const char *fmt, va_list args) {
	return vprintf(fmt, args);
}

u_char *
stu_vsprintf(u_char *s, const char *fmt, va_list args) {
	stu_int_t  n;
	u_char    *p = s;

	n = vsprintf((char *) s, fmt, args);
	if (n < 0) {
		return NULL;
	}
	p += n;

	return p;
}


uint32_t stu_utf8_decode(u_char **p, size_t n) {
	size_t    len;
	uint32_t  u, i, valid;

	u = **p;
	if (u >= 0xf0) {
		u &= 0x07;
		valid = 0xffff;
		len = 3;
	} else if (u >= 0xe0) {
		u &= 0x0f;
		valid = 0x7ff;
		len = 2;
	} else if (u >= 0xc2) {
		u &= 0x1f;
		valid = 0x7f;
		len = 1;
	} else {
		return 0xffffffff;
	}

	if (n - 1 < len) {
		return 0xfffffffe;
	}

	(*p)++;

	while (len) {
		i = *(*p)++;

		if (i < 0x80) {
			return 0xffffffff;
		}

		u = (u << 6) | (i & 0x3f);

		len--;
	}

	if (u > valid) {
		return u;
	}

	return 0xffffffff;
}

void
stu_unescape_uri(u_char **dst, u_char **src, size_t size, stu_uint_t type) {
	u_char  *d, *s, ch, c, decoded;
	enum {
		sw_usual = 0,
		sw_quoted,
		sw_quoted_second
	} state;

	d = *dst;
	s = *src;

	state = 0;
	decoded = 0;

	while (size--) {
		ch = *s++;

		switch (state) {
		case sw_usual:
			if (ch == '?' && (type & (STU_UNESCAPE_URI | STU_UNESCAPE_REDIRECT))) {
				*d++ = ch;
				goto done;
			}

			if (ch == '%' && size > 1) {
				ch = *s;
				c = (u_char) (ch | 0x20);

				if ((ch >= '0' && ch <= '9') || (c >= 'a' && c <= 'f')) {
					ch = *(s + 1);
					c = (u_char) (ch | 0x20);

					if ((ch >= '0' && ch <= '9') || (c >= 'a' && c <= 'f')) {
						state = sw_quoted;
						break;
					}
				}

				*d++ = '%';
				break;
			}

			if (ch == '+') {
				*d++ = ' ';
				break;
			}

			*d++ = ch;
			break;

		case sw_quoted:
			if (ch >= '0' && ch <= '9') {
				decoded = (u_char) (ch - '0');
				state = sw_quoted_second;
				break;
			}

			c = (u_char) (ch | 0x20);
			if (c >= 'a' && c <= 'f') {
				decoded = (u_char) (c - 'a' + 10);
				state = sw_quoted_second;
				break;
			}

			/* the invalid quoted character */
			state = sw_usual;
			*d++ = ch;
			break;

		case sw_quoted_second:
			state = sw_usual;

			if (ch >= '0' && ch <= '9') {
				ch = (u_char) ((decoded << 4) + ch - '0');

				if (type & STU_UNESCAPE_REDIRECT) {
					if (ch > '%' && ch < 0x7f) {
						*d++ = ch;
						break;
					}

					*d++ = '%';
					*d++ = *(s - 2);
					*d++ = *(s - 1);
					break;
				}

				*d++ = ch;
				break;
			}

			c = (u_char) (ch | 0x20);
			if (c >= 'a' && c <= 'f') {
				ch = (u_char) ((decoded << 4) + c - 'a' + 10);

				if (type & STU_UNESCAPE_URI) {
					if (ch == '?') {
						*d++ = ch;
						goto done;
					}

					*d++ = ch;
					break;
				}

				if (type & STU_UNESCAPE_REDIRECT) {
					if (ch == '?') {
						*d++ = ch;
						goto done;
					}

					if (ch > '%' && ch < 0x7f) {
						*d++ = ch;
						break;
					}

					*d++ = '%';
					*d++ = *(s - 2);
					*d++ = *(s - 1);
					break;
				}

				*d++ = ch;
				break;
			}

			/* the invalid quoted character */
			break;
		}
	}

done:

	*dst = d;
	*src = s;
}

