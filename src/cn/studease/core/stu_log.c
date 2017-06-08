/*
 * stu_log.c
 *
 *  Created on: 2016-9-8
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"
#include <stdio.h>

stu_file_t *stu_log = NULL;

static u_char *stu_log_prefix(u_char *buf, const stu_str_t prefix);
static u_char *stu_log_errno(u_char *buf, u_char *last, stu_int_t err);


stu_int_t
stu_log_init(stu_file_t *file) {
	u_char    *path, *p, *last, temp[STU_FILE_PATH_MAX_LEN];
	stu_dir_t  dir;

	path = NULL;

	last = file->name.data + file->name.len;
	p = stu_strrchr(file->name.data, last, '/');
	if (p) {
		stu_strncpy(temp, file->name.data, p - file->name.data);
		path = temp;
	}

	if (path) {
		if (stu_open_dir(path, &dir) == STU_ERROR) {
			stu_log_debug(3, "Failed to " stu_open_file_n " log file dir.");

			if (stu_create_dir(path, STU_FILE_DEFAULT_ACCESS) == STU_ERROR) {
				stu_log_error(stu_errno, "Failed to " stu_create_dir_n " for log file.");
				return STU_ERROR;
			}
		}
	}

	file->fd = stu_open_file(file->name.data, STU_FILE_APPEND, STU_FILE_CREATE_OR_OPEN, STU_FILE_DEFAULT_ACCESS);
	if (file->fd == STU_FILE_INVALID) {
		stu_log_error(stu_errno, "Failed to " stu_open_file_n " log file \"%s\".", file->name.data);
		return STU_ERROR;
	}

	if (path) {
		if (stu_close_dir(&dir) == -1) {
			stu_log_error(stu_errno, "Failed to " stu_close_file_n " log file dir.");
			return STU_ERROR;
		}
	}

	stu_log = file;

	return STU_OK;
}

void
stu_log_info(const char *fmt, ...) {
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

	if (level < __LOGGER) {
		return;
	}

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


static u_char *
stu_log_prefix(u_char *buf, const stu_str_t prefix) {
	u_char         *p;
	struct timeval  tv;
	stu_tm_t        tm;

	p = buf;

	stu_gettimeofday(&tv);
	stu_localtime(tv.tv_sec, &tm);

	p = stu_sprintf(p, "[%4d-%02d-%02d %02d:%02d:%02d]",
			tm.stu_tm_year, tm.stu_tm_mon, tm.stu_tm_mday,
			tm.stu_tm_hour, tm.stu_tm_min, tm.stu_tm_sec
		);

	memcpy(p, prefix.data, prefix.len);
	p += prefix.len;

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
	if (buf < last) {
		*buf++ = ')';
	}

	*buf = '\0';

	return buf;
}

