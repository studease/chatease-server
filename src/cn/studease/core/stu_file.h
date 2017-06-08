/*
 * stu_file.h
 *
 *  Created on: 2016-9-21
 *      Author: root
 */

#ifndef STU_FILE_H_
#define STU_FILE_H_

#include <dirent.h>
#include <sys/stat.h>
#include "stu_config.h"
#include "stu_core.h"

#define STU_FILE_INVALID         -1
#define STU_FILE_PATH_MAX_LEN    128

typedef int                      stu_fd_t;
typedef struct stat              stu_file_info_t;
typedef struct stu_file_s        stu_file_t;

struct stu_file_s {
	stu_fd_t                      fd;
	stu_str_t                     name;
	stu_file_info_t               info;

	void                        (*flush)(stu_file_t *file);
	void                         *data;

	off_t                         offset;
};

typedef struct {
    DIR                         *dir;
    struct dirent               *de;
    struct stat                  info;

    unsigned                     type:8;
    unsigned                     valid_info:1;
} stu_dir_t;

#define STU_FILE_RDONLY          O_RDONLY
#define STU_FILE_WRONLY          O_WRONLY
#define STU_FILE_RDWR            O_RDWR
#define STU_FILE_CREATE_OR_OPEN  O_CREAT
#define STU_FILE_OPEN            0
#define STU_FILE_TRUNCATE       (O_CREAT | O_TRUNC)
#define STU_FILE_APPEND         (O_WRONLY | O_APPEND)
#define STU_FILE_NONBLOCK        O_NONBLOCK

#define STU_FILE_DEFAULT_ACCESS  0644
#define STU_FILE_OWNER_ACCESS    0600

#ifdef __CYGWIN__

#define stu_open_file(name, mode, create, access)                            \
	open((const char *) name, mode|create|O_BINARY, access)

#else

#define stu_open_file(name, mode, create, access)                            \
	open((const char *) name, mode|create, access)

#endif

#define stu_open_file_n          "open()"

#define stu_close_file           close
#define stu_close_file_n         "close()"

#define stu_delete_file(name)    unlink((const char *) name)
#define stu_delete_file_n        "unlink()"


ssize_t stu_read_file(stu_file_t *file, u_char *buf, size_t size, off_t offset);
#if (STU_HAVE_PREAD)
#define stu_read_file_n          "pread()"
#else
#define stu_read_file_n          "read()"
#endif

ssize_t stu_write_file(stu_file_t *file, u_char *buf, size_t size, off_t offset);

#define stu_read_fd              read
#define stu_read_fd_n            "read()"

/*
 * we use inlined function instead of simple #define
 * because glibc 2.3 sets warn_unused_result attribute for write()
 * and in this case gcc 4.3 ignores (void) cast
 */
static stu_inline ssize_t
stu_write_fd(stu_fd_t fd, void *buf, size_t n) {
	return write(fd, buf, n);
}

#define stu_write_fd_n           "write()"


#define stu_rename_file(o, n)    rename((const char *) o, (const char *) n)
#define stu_rename_file_n        "rename()"

#define stu_change_file_access(n, a) chmod((const char *) n, a)
#define stu_change_file_access_n "chmod()"

stu_int_t stu_set_file_time(u_char *name, stu_fd_t fd, time_t s);
#define stu_set_file_time_n      "utimes()"

#define stu_file_info(file, sb)  stat((const char *) file, sb)
#define stu_file_info_n          "stat()"

#define stu_fd_info(fd, sb)      fstat(fd, sb)
#define stu_fd_info_n            "fstat()"

#define stu_link_info(file, sb)  lstat((const char *) file, sb)
#define stu_link_info_n          "lstat()"

#define stu_is_dir(sb)           (S_ISDIR((sb)->st_mode))
#define stu_is_file(sb)          (S_ISREG((sb)->st_mode))
#define stu_is_link(sb)          (S_ISLNK((sb)->st_mode))
#define stu_is_exec(sb)          (((sb)->st_mode & S_IXUSR) == S_IXUSR)
#define stu_file_access(sb)      ((sb)->st_mode & 0777)
#define stu_file_size(sb)        (sb)->st_size
#define stu_file_fs_size(sb)     stu_max((sb)->st_size, (sb)->st_blocks * 512)
#define stu_file_mtime(sb)       (sb)->st_mtime
#define stu_file_uniq(sb)        (sb)->st_ino


#define stu_realpath(p, r)       (u_char *) realpath((char *) p, (char *) r)
#define stu_realpath_n           "realpath()"
#define stu_getcwd(buf, size)    (getcwd((char *) buf, size) != NULL)
#define stu_getcwd_n             "getcwd()"
#define stu_path_separator(c)    ((c) == '/')


stu_int_t stu_open_dir(u_char *name, stu_dir_t *dir);
#define stu_open_dir_n           "opendir()"

#define stu_close_dir(d)         closedir((d)->dir)
#define stu_close_dir_n          "closedir()"

stu_int_t stu_read_dir(stu_dir_t *dir);
#define stu_read_dir_n           "readdir()"

#define stu_create_dir(name, access) mkdir((const char *) name, access)
#define stu_create_dir_n         "mkdir()"

#define stu_delete_dir(name)     rmdir((const char *) name)
#define stu_delete_dir_n         "rmdir()"

#define stu_dir_access(a)        (a | (a & 0444) >> 2)


#define stu_de_name(dir)         ((u_char *) (dir)->de->d_name)
#if (STU_HAVE_D_NAMLEN)
#define stu_de_namelen(dir)      (dir)->de->d_namlen
#else
#define stu_de_namelen(dir)      stu_strlen((dir)->de->d_name)
#endif

static stu_inline stu_int_t
stu_de_info(u_char *name, stu_dir_t *dir) {
    dir->type = 0;
    return stat((const char *) name, &dir->info);
}

#define stu_de_info_n            "stat()"
#define stu_de_link_info(name, dir)  lstat((const char *) name, &(dir)->info)
#define stu_de_link_info_n       "lstat()"

#if (STU_HAVE_D_TYPE)

/*
 * some file systems (e.g. XFS on Linux and CD9660 on FreeBSD)
 * do not set dirent.d_type
 */

#define stu_de_is_dir(dir)                                                   \
    (((dir)->type) ? ((dir)->type == DT_DIR) : (S_ISDIR((dir)->info.st_mode)))
#define stu_de_is_file(dir)                                                  \
    (((dir)->type) ? ((dir)->type == DT_REG) : (S_ISREG((dir)->info.st_mode)))
#define stu_de_is_link(dir)                                                  \
    (((dir)->type) ? ((dir)->type == DT_LNK) : (S_ISLNK((dir)->info.st_mode)))

#else

#define stu_de_is_dir(dir)       (S_ISDIR((dir)->info.st_mode))
#define stu_de_is_file(dir)      (S_ISREG((dir)->info.st_mode))
#define stu_de_is_link(dir)      (S_ISLNK((dir)->info.st_mode))

#endif

#define stu_de_access(dir)       (((dir)->info.st_mode) & 0777)
#define stu_de_size(dir)         (dir)->info.st_size
#define stu_de_fs_size(dir)                                                  \
    stu_max((dir)->info.st_size, (dir)->info.st_blocks * 512)
#define stu_de_mtime(dir)        (dir)->info.st_mtime


stu_int_t stu_trylock_fd(stu_fd_t fd);
stu_int_t stu_lock_fd(stu_fd_t fd);
stu_int_t stu_unlock_fd(stu_fd_t fd);

#define stu_trylock_fd_n         "fcntl(F_SETLK, F_WRLCK)"
#define stu_lock_fd_n            "fcntl(F_SETLKW, F_WRLCK)"
#define stu_unlock_fd_n          "fcntl(F_SETLK, F_UNLCK)"


#define stu_stderr               STDERR_FILENO

#endif /* STU_FILE_H_ */
