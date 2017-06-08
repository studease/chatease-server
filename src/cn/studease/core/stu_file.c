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
		stu_log_error(stu_errno, "pread() \"%s\" failed.", file->name.data);
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
			stu_log_error(stu_errno, "pwrite() \"%s\" failed.", file->name.data);
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


stu_int_t
stu_set_file_time(u_char *name, stu_fd_t fd, time_t s) {
	struct timeval  tv[2];

	tv[0].tv_sec = s;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = s;
	tv[1].tv_usec = 0;

	if (utimes((char *) name, tv) != -1) {
		return STU_OK;
	}

	return STU_ERROR;
}


stu_int_t
stu_open_dir(u_char *name, stu_dir_t *dir) {
	dir->dir = opendir((const char *) name);

	if (dir->dir == NULL) {
		return STU_ERROR;
	}

	dir->valid_info = 0;

	return STU_OK;
}

stu_int_t
stu_read_dir(stu_dir_t *dir) {
	dir->de = readdir(dir->dir);

	if (dir->de) {
#if (STU_HAVE_D_TYPE)
		dir->type = dir->de->d_type;
#else
		dir->type = 0;
#endif
		return STU_OK;
	}

	return STU_ERROR;
}


stu_int_t
stu_trylock_fd(stu_fd_t fd) {
	struct flock  fl;

	stu_memzero(&fl, sizeof(struct flock));
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;

	if (fcntl(fd, F_SETLK, &fl) == -1) {
		stu_log_error(stu_errno, "fcntl() \"%d\" failed.", fd);
		return STU_ERROR;
	}

	return STU_OK;
}

stu_int_t
stu_lock_fd(stu_fd_t fd) {
	struct flock  fl;

	stu_memzero(&fl, sizeof(struct flock));
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;

	if (fcntl(fd, F_SETLKW, &fl) == -1) {
		stu_log_error(stu_errno, "fcntl() \"%d\" failed.", fd);
		return STU_ERROR;
	}

	return STU_OK;
}

stu_int_t
stu_unlock_fd(stu_fd_t fd) {
	struct flock  fl;

	stu_memzero(&fl, sizeof(struct flock));
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;

	if (fcntl(fd, F_SETLK, &fl) == -1) {
		stu_log_error(stu_errno, "fcntl() \"%d\" failed.", fd);
		return STU_ERROR;
	}

	return STU_OK;
}
