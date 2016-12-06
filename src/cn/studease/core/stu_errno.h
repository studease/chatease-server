/*
 * stu_errno.h
 *
 *  Created on: 2016-9-8
 *      Author: Tony Lau
 */

#ifndef STU_ERRNO_H_
#define STU_ERRNO_H_

#include "stu_config.h"
#include "stu_core.h"

#ifndef STU_SYS_NERR
#define STU_SYS_NERR  132
#endif

#define stu_errno           errno
#define stu_set_errno(err)  errno = err

typedef struct {
	u_char     id;
	stu_str_t *explain;
} stu_error_t;

u_char *stu_strerror(stu_int_t err, u_char *errstr, size_t size);
stu_int_t stu_strerror_init(void);

#endif /* STU_ERRNO_H_ */
