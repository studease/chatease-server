/*
 * stu_socket.h
 *
 *  Created on: 2016-9-9
 *      Author: root
 */

#ifndef STU_SOCKET_H_
#define STU_SOCKET_H_

#include <fcntl.h>
#include "stu_config.h"
#include "stu_core.h"

typedef int  stu_socket_t;

#define stu_nonblocking(s)  fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK)
#define stu_close_socket    close

#endif /* STU_SOCKET_H_ */
