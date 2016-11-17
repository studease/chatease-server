/*
 * stu_filedes.h
 *
 *  Created on: 2016-9-21
 *      Author: Tony Lau
 */

#ifndef STU_FILEDES_H_
#define STU_FILEDES_H_

#include "stu_config.h"
#include "stu_core.h"

typedef struct {
     stu_uint_t  command;
     stu_pid_t   pid;
     stu_int_t   slot;
     stu_fd_t    fd;
} stu_filedes_t;


stu_int_t stu_filedes_write(stu_socket_t s, stu_filedes_t *fds, size_t size);
stu_int_t stu_filedes_read(stu_socket_t s, stu_filedes_t *fds, size_t size);
stu_int_t stu_filedes_add_event(stu_fd_t fd, uint32_t event, stu_event_handler_pt handler);
void stu_filedes_close(stu_fd_t *fds);

#endif /* STU_FILEDES_H_ */
