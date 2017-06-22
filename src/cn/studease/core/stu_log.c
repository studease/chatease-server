/*
 * stu_log.c
 *
 *  Created on: 2016-9-8
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"
#include <stdio.h>

stu_file_t *stu_logger = NULL;

static u_char *stu_log_prefix(u_char *buf, const stu_str_t prefix);
static u_char *stu_log_errno(u_char *buf, u_char *last, stu_int_t err);


stu_int_t
stu_log_init(stu_file_t *file) {
	u_char    *p, *last, temp[STU_FILE_PATH_MAX_LEN];

	last = file->name.data + file->name.len;

	p = stu_strlchr(file->name.data, last, '/');
	while (p) {
		stu_strncpy(temp, file->name.data, p - file->name.data);
		if (stu_file_exist(temp) == -1) {
			if (stu_dir_create(temp, STU_FILE_DEFAULT_ACCESS) == STU_ERROR) {
				stu_log_error(stu_errno, "Failed to " stu_dir_create_n " for log file.");
				return STU_ERROR;
			}
		}

		p = stu_strlchr(p + 1, last, '/');
	}

	file->fd = stu_file_open(file->name.data, STU_FILE_APPEND, STU_FILE_CREATE_OR_OPEN, STU_FILE_DEFAULT_ACCESS);
	if (file->fd == STU_FILE_INVALID) {
		stu_log_error(stu_errno, "Failed to " stu_file_open_n " log file \"%s\".", file->name.data);
		return STU_ERROR;
	}

	stu_logger = file;

	return STU_OK;
}

void
stu_log_c(const char *fmt, ...) {
	u_char   temp[STU_LOG_RECORD_MAX_LEN];
	u_char  *p, *last = temp + STU_LOG_RECORD_MAX_LEN;
	va_list  args;

	p = stu_log_prefix(temp, STU_LOG_PREFIX);

	va_start(args, fmt);
	p = stu_vsprintf(p, fmt, args);
	va_end(args);

	if (p >= last - 1) {
		p = last - 2;
	}
	*p++ = LF;
	*p = '\0';

	if (stu_logger) {
		stu_file_write(stu_logger, temp, p - temp, stu_atomic_fetch(&stu_logger->offset));
	}

	stu_printf((const char *) temp);
}

void
stu_log_c_debug(stu_int_t level, const char *fmt, ...) {
	u_char   temp[STU_LOG_RECORD_MAX_LEN];
	u_char  *p, *last = temp + STU_LOG_RECORD_MAX_LEN;
	va_list  args;

	if (level < __LOGGER) {
		return;
	}

	p = stu_log_prefix(temp, STU_DEBUG_PREFIX);
	p = stu_sprintf(p, "[%d]", level);

	va_start(args, fmt);
	p = stu_vsprintf(p, fmt, args);
	va_end(args);

	if (p >= last - 1) {
		p = last - 2;
	}
	*p++ = LF;
	*p = '\0';

	if (stu_logger) {
		stu_file_write(stu_logger, temp, p - temp, stu_atomic_fetch(&stu_logger->offset));
	}

	stu_printf((const char *) temp);
}

void
stu_log_c_error(stu_int_t err, const char *fmt, ...) {
	u_char   temp[STU_LOG_RECORD_MAX_LEN];
	u_char  *p, *last = temp + STU_LOG_RECORD_MAX_LEN;
	va_list  args;

	p = stu_log_prefix(temp, STU_ERROR_PREFIX);

	va_start(args, fmt);
	p = stu_vsprintf(p, fmt, args);
	va_end(args);

	if (err) {
		p = stu_log_errno(p, last, err);
	} else {
		*p++ = LF;
	}

	if (p >= last - 1) {
		p = last - 2;
	}
	*p++ = LF;
	*p = '\0';

	if (stu_logger) {
		stu_file_write(stu_logger, temp, p - temp, stu_atomic_fetch(&stu_logger->offset));
	}

	stu_printf((const char *) temp);
}


static u_char *
stu_log_prefix(u_char *buf, const stu_str_t prefix) {
	u_char *p;

	p = stu_memcpy(buf, stu_cached_log_time.data, stu_cached_log_time.len);
	p = stu_memcpy(p, prefix.data, prefix.len);

	return p;
}

static u_char *
stu_log_errno(u_char *buf, u_char *last, stu_int_t err) {
	if (buf > last - 50) {
		buf = last - 50;
		*buf++ = '.';
		*buf++ = '.';
		*buf++ = '.';
	}

	buf = stu_sprintf(buf, " (%d: ", err);

	buf = stu_strerror(err, buf, last - buf);
	if (buf < last - 1) {
		*buf++ = ')';
	}
	*buf++ = LF;
	*buf = '\0';

	return buf;
}

