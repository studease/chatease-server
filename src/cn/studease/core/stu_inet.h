/*
 * stu_inet.h
 *
 *  Created on: 2017-3-14
 *      Author: Tony Lau
 */

#ifndef STU_INET_H_
#define STU_INET_H_

#include "stu_config.h"
#include "stu_core.h"

typedef struct {
	struct sockaddr  sockaddr;
	socklen_t        socklen;
	stu_str_t        name;
} stu_addr_t;

#endif /* STU_INET_H_ */
