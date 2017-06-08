/*
 * stu_conf_file.c
 *
 *  Created on: 2017-6-7
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_str_t  STU_CONF_FILE_DEFAULT_PATH = stu_string("conf/chatd.conf");


stu_int_t
stu_conf_file_parse(u_char *name) {
	u_char      temp[STU_CONF_FILE_MAX_SIZE];
	stu_file_t  conf;

	conf.fd = stu_open_file(name, STU_FILE_RDONLY, STU_FILE_CREATE_OR_OPEN, STU_FILE_DEFAULT_ACCESS);
	if (conf.fd == STU_FILE_INVALID) {
		stu_log_error(stu_errno, "Failed to " stu_open_file_n " conf file \"%s\".", name);
		return STU_ERROR;
	}

	stu_memzero(temp, STU_CONF_FILE_MAX_SIZE);
	if (stu_read_file(&conf, temp, STU_CONF_FILE_MAX_SIZE, conf.offset) == STU_ERROR) {
		stu_log_error(stu_errno, "Failed to " stu_read_file_n " conf file \"%s\".", name);
		return STU_ERROR;
	}

	return STU_OK;
}
