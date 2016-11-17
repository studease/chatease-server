/*
 * stu_errno.c
 *
 *  Created on: 2016-9-8
 *      Author: Tony Lau
 */

#include <stdio.h>
#include <stdlib.h>
#include "stu_config.h"
#include "stu_core.h"

static stu_str_t  *stu_sys_errlist;
static stu_str_t   stu_unknown_error = stu_string("Unknown error");


u_char *
stu_strerror(stu_int_t err, u_char *errstr, size_t size) {
	 stu_str_t  *msg;

	 msg = ((stu_uint_t) err < STU_SYS_NERR) ? &stu_sys_errlist[err]: &stu_unknown_error;
	 size = stu_min(size, msg->len);

	 return stu_memcpy(errstr, msg->data, size);
}

stu_int_t
stu_strerror_init(void) {
	char      *msg;
	u_char    *p;
	size_t     len;
	stu_int_t  err;

	len = STU_SYS_NERR * sizeof(stu_str_t);

	stu_sys_errlist = malloc(len);
	if (stu_sys_errlist == NULL) {
		goto failed;
	}

	for (err = 0; err < STU_SYS_NERR; err++) {
		msg = strerror(err);
		len = stu_strlen(msg);

		p = malloc(len);
		if (p == NULL) {
			goto failed;
		}

		memcpy(p, msg, len);
		stu_sys_errlist[err].len = len;
		stu_sys_errlist[err].data = p;
	}

	return STU_OK;

failed:

	err = errno;
	stu_log_error(0, "malloc(%d) failed (%d: %s)", len, err, strerror(err));

	return STU_ERROR;
}

