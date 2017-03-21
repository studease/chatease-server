/*
 * stu_file.h
 *
 *  Created on: 2016-9-21
 *      Author: root
 */

#ifndef STU_FILE_H_
#define STU_FILE_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_FILE_RDONLY          O_RDONLY
#define STU_FILE_WRONLY          O_WRONLY
#define STU_FILE_RDWR            O_RDWR
#define STU_FILE_CREATE_OR_OPEN  O_CREAT
#define STU_FILE_OPEN            0
#define STU_FILE_TRUNCATE       (O_CREAT | O_TRUNC)
#define STU_FILE_APPEND         (O_WRONLY | O_APPEND)
#define STU_FILE_NONBLOCK        O_NONBLOCK

#ifdef __CYGWIN__

#define stu_open_file(name, mode, create, access)                            \
	open((const char *) name, mode|create|O_BINARY, access)

#else

#define stu_open_file(name, mode, create, access)                            \
	open((const char *) name, mode|create, access)

#endif

#define stu_close_file           close
#define stu_delete_file(name)    unlink((const char *) name)

#define stu_rename_file(o, n)    rename((const char *) o, (const char *) n)


typedef int         stu_fd_t;
typedef struct stat stu_file_info_t;

typedef struct {
	stu_fd_t         fd;
	stu_str_t        name;
	stu_file_info_t  info;

	off_t            offset;
} stu_file_t;

#define stu_read_fd              read

static stu_inline ssize_t
stu_write_fd(stu_fd_t fd, void *buf, size_t n) {
	return write(fd, buf, n);
}

ssize_t stu_read_file(stu_file_t *file, u_char *buf, size_t size, off_t offset);
ssize_t stu_write_file(stu_file_t *file, u_char *buf, size_t size, off_t offset);

#endif /* STU_FILE_H_ */
