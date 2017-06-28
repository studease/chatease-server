/*
 * stu_conf_file.h
 *
 *  Created on: 2017-6-7
 *      Author: Tony Lau
 */

#ifndef STU_CONF_FILE_H_
#define STU_CONF_FILE_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_CONF_FILE_MAX_SIZE  2048

typedef struct {
	stu_file_t   file;
	stu_buf_t   *buffer;
} stu_conf_file_t;

typedef struct {
	stu_str_t    name;
	stu_short_t  mask;
} stu_conf_bitmask_t;

stu_int_t stu_conf_file_parse(stu_config_t *cf, u_char *name, stu_pool_t *pool);

#endif /* STU_CONF_FILE_H_ */
