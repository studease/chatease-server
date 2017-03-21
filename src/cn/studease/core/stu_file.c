/*
 * stu_file.c
 *
 *  Created on: 2017-3-15
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

ssize_t
stu_read_file(stu_file_t *file, u_char *buf, size_t size, off_t offset) {
	ssize_t  n;

	n = pread(file->fd, buf, size, offset);
	if (n == -1) {
		stu_log_error(stu_errno, "pread() \"%s\" failed", file->name.data);
		return STU_ERROR;
	}

	file->offset += n;

	return n;
}

ssize_t
stu_write_file(stu_file_t *file, u_char *buf, size_t size, off_t offset) {
	ssize_t  n, written;

	written = 0;

	for ( ;; ) {
		n = pwrite(file->fd, buf + written, size, offset);
		if (n == -1) {
			stu_log_error(stu_errno, "pwrite() \"%s\" failed", file->name.data);
			return STU_ERROR;
		}

		file->offset += n;
		written += n;

		if ((size_t) n == size) {
			break;
		}

		offset += n;
		size -= n;
	}

	return written;
}
