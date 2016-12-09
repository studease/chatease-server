/*
 * stu_log.c
 *
 *  Created on: 2016-9-8
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"
#include <stdio.h>

u_char *stu_log_prefix(u_char *buf, const stu_str_t prefix);
u_char *stu_log_errno(u_char *buf, u_char *last, stu_int_t err);


void
stu_log(const char *fmt, ...) {
	u_char   temp[STU_LOG_RECORD_MAX_LEN];
	u_char  *p, *last = temp + STU_LOG_RECORD_MAX_LEN;
	va_list  args;

	p = stu_log_prefix(temp, STU_LOG_PREFIX);

	va_start(args, fmt);
	p = stu_vsprintf(p, fmt, args);
	va_end(args);

	if (p >= last) {
		p = last - 1;
	}

	stu_printf("%s\n", temp);
}

void
stu_log_debug(stu_int_t level, const char *fmt, ...) {
	u_char   temp[STU_LOG_RECORD_MAX_LEN];
	u_char  *p, *last = temp + STU_LOG_RECORD_MAX_LEN;
	va_list  args;

	p = stu_log_prefix(temp, STU_DEBUG_PREFIX);
	p = stu_sprintf(p, "%d: ", level);

	va_start(args, fmt);
	p = stu_vsprintf(p, fmt, args);
	va_end(args);

	if (p >= last) {
		p = last - 1;
	}

	stu_printf("%s\n", temp);
}

void
stu_log_error(stu_int_t err, const char *fmt, ...) {
	u_char   temp[STU_LOG_RECORD_MAX_LEN];
	u_char  *p, *last = temp + STU_LOG_RECORD_MAX_LEN;
	va_list  args;

	p = stu_log_prefix(temp, STU_ERROR_PREFIX);

	va_start(args, fmt);
	p = stu_vsprintf(p, fmt, args);
	va_end(args);

	if (err) {
		p = stu_log_errno(p, last, err);
	}

	if (p >= last) {
		p = last - 1;
	}

	stu_printf("%s\n", temp);
}

u_char *
stu_log_prefix(u_char *buf, const stu_str_t prefix) {
	u_char *p = buf;

	memcpy(buf, prefix.data, prefix.len);
	p = buf + prefix.len;

	return p;
}

u_char *
stu_log_errno(u_char *buf, u_char *last, stu_int_t err) {
	if (buf > last - 50) {
		buf = last - 50;
		*buf++ = '.';
		*buf++ = '.';
		*buf++ = '.';
	}

	buf = stu_sprintf(buf, " (%d: ", err);

	buf = stu_strerror(err, buf, last - buf);
	if (buf < last) {
		*buf++ = ')';
	}

	*buf = '\0';

	return buf;
}

