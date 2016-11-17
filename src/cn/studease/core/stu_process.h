/*
 * stu_process.h
 *
 *  Created on: 2016-9-12
 *      Author: Tony Lau
 */

#ifndef STU_PROCESS_H_
#define STU_PROCESS_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_PROCESSES_MAXIMUM  32

#define STU_CMD_OPEN_FILEDES   1
#define STU_CMD_CLOSE_FILEDES  2
#define STU_CMD_SEND_MESSAGE   3
#define STU_CMD_QUIT           4
#define STU_CMD_RESTART        5

#define STU_INVALID_PID        -1

typedef pid_t stu_pid_t;
typedef void  (*stu_spawn_proc_pt)(stu_cycle_t *cycle, void *data);

typedef struct {
	stu_pid_t          pid;
	stu_socket_t       filedes[2];

	stu_spawn_proc_pt  proc;
	void              *data;
	char              *name;

	stu_int_t          status;
	stu_int_t          state;
} stu_process_t;

#define stu_getpid         getpid

#include <sched.h>
#define stu_sched_yield()  sched_yield()

void stu_start_worker_processes(stu_cycle_t *cycle);
stu_pid_t stu_spawn_process(stu_cycle_t *cycle, stu_spawn_proc_pt proc, void *data, char *name);

#endif /* STU_PROCESS_H_ */
