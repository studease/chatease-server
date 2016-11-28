/*
 * stu_buf.h
 *
 *  Created on: 2016-11-15
 *      Author: root
 */

#ifndef STU_BUF_H_
#define STU_BUF_H_

#include "stu_config.h"
#include "stu_core.h"

typedef struct {
	u_char *start;
	u_char *last;
	u_char *end;
} stu_buf_t;

#endif /* STU_BUF_H_ */
