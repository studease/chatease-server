/*
 * stu_process.c
 *
 *  Created on: 2016-9-13
 *      Author: Tony Lau
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include "stu_config.h"
#include "stu_core.h"

stu_pid_t      stu_pid;
stu_int_t      stu_process_slot;
stu_socket_t   stu_filedes;
stu_int_t      stu_last_process;
stu_process_t  stu_processes[STU_PROCESSES_MAXIMUM];

sig_atomic_t   stu_quit;
sig_atomic_t   stu_restart;

stu_thread_t   stu_threads[STU_THREADS_MAXIMUM];
stu_int_t      stu_threads_n;

static void  stu_pass_open_filedes(stu_cycle_t *cycle, stu_filedes_t *fds);
static void  stu_worker_process_cycle(stu_cycle_t *cycle, void *data);
static void  stu_worker_process_init(stu_cycle_t *cycle, stu_int_t worker);
static void  stu_filedes_handler(stu_event_t *ev);
static void *stu_worker_thread_cycle(void *data);


void
stu_start_worker_processes(stu_cycle_t *cycle) {
	stu_filedes_t  fds;
	stu_config_t  *cf;
	stu_int_t      i;

	stu_memzero(&fds, sizeof(stu_filedes_t));
	fds.command = STU_CMD_OPEN_FILEDES;

	cf = &cycle->config;
	for (i = 0; i < cf->worker_processes; i++) {
		stu_spawn_process(cycle, stu_worker_process_cycle, (void *) (intptr_t) i, "worker process");

		fds.pid = stu_processes[stu_process_slot].pid;
		fds.slot = stu_process_slot;
		fds.fd = stu_processes[stu_process_slot].filedes[0];

		stu_pass_open_filedes(cycle, &fds);
	}

	// main thread of master process, wait for signal
	for ( ;; ) {
		sleep(5);
	}
}

stu_pid_t
stu_spawn_process(stu_cycle_t *cycle, stu_spawn_proc_pt proc, void *data, char *name) {
	u_long     on;
	stu_pid_t  pid;
	stu_int_t  s;

	for (s = 0; s < stu_last_process; s++) {
		if (stu_processes[s].state == 0) {
			break;
		}
	}

	if (s == STU_PROCESSES_MAXIMUM) {
		stu_log_error(0, "no more than %d processes can be spawned", STU_PROCESSES_MAXIMUM);
		return STU_INVALID_PID;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, stu_processes[s].filedes) == -1) {
		stu_log_error(stu_errno, "socketpair() failed while spawning \"%s\"", name);
		return STU_INVALID_PID;
	}

	stu_log_debug(0, "filedes %d:%d", stu_processes[s].filedes[0], stu_processes[s].filedes[1]);

	if (stu_nonblocking(stu_processes[s].filedes[0]) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while spawning \"%s\"", name);
		stu_filedes_close(stu_processes[s].filedes);
		return STU_INVALID_PID;
	}

	if (stu_nonblocking(stu_processes[s].filedes[1]) == -1) {
		stu_log_error(stu_errno, "fcntl(O_NONBLOCK) failed while spawning \"%s\"", name);
		stu_filedes_close(stu_processes[s].filedes);
		return STU_INVALID_PID;
	}

	on = 1;
	if (ioctl(stu_processes[s].filedes[0], FIOASYNC, &on) == -1) {
		stu_log_error(stu_errno, "ioctl(FIOASYNC) failed while spawning \"%s\"", name);
		stu_filedes_close(stu_processes[s].filedes);
		return STU_INVALID_PID;
	}

	if (fcntl(stu_processes[s].filedes[0], F_SETOWN, stu_pid) == -1) {
		stu_log_error(stu_errno, "fcntl(F_SETOWN) failed while spawning \"%s\"", name);
		stu_filedes_close(stu_processes[s].filedes);
		return STU_INVALID_PID;
	}

	if (fcntl(stu_processes[s].filedes[0], F_SETFD, FD_CLOEXEC) == -1) {
		stu_log_error(stu_errno, "fcntl(FD_CLOEXEC) failed while spawning \"%s\"", name);
		stu_filedes_close(stu_processes[s].filedes);
		return STU_INVALID_PID;
	}

	if (fcntl(stu_processes[s].filedes[1], F_SETFD, FD_CLOEXEC) == -1) {
		stu_log_error(stu_errno, "fcntl(FD_CLOEXEC) failed while spawning \"%s\"", name);
		stu_filedes_close(stu_processes[s].filedes);
		return STU_INVALID_PID;
	}

	stu_filedes = stu_processes[s].filedes[1];
	stu_process_slot = s;

	pid = fork();

	switch (pid) {
	case -1:
		stu_log_error(stu_errno, "fork() failed while spawning \"%s\"", name);
		stu_filedes_close(stu_processes[s].filedes);
		return STU_INVALID_PID;
	case 0:
		stu_pid = stu_getpid();
		proc(cycle, data);
		break;
	default:
		break;
	}

	stu_log_debug(0, "start \"%s\", pid: %d", name, pid);

	stu_processes[s].pid = pid;
	stu_processes[s].proc = proc;
	stu_processes[s].data = data;
	stu_processes[s].name = name;
	stu_processes[s].status = 0;
	stu_processes[s].state = 1;

	if (s == stu_last_process) {
		stu_last_process++;
	}

	return pid;
}

static void
stu_pass_open_filedes(stu_cycle_t *cycle, stu_filedes_t *fds) {
	stu_int_t  i;

	for (i = 0; i < stu_last_process; i++) {
		if (i == stu_process_slot || stu_processes[i].state == 0 || stu_processes[i].filedes[0] == 0) {
			continue;
		}

		stu_log_debug(0, "pass filedes s:%d pid:%lu fd:%d to s:%d pid:%lu fd:%d",
				fds->slot, fds->pid, fds->fd, i, stu_processes[i].pid, stu_processes[i].filedes[0]);

		stu_filedes_write(stu_processes[i].filedes[0], fds, sizeof(stu_filedes_t));
	}
}

static void
stu_worker_process_cycle(stu_cycle_t *cycle, void *data) {
	stu_int_t  worker = (intptr_t) data;
	stu_int_t  threads_n = cycle->config.worker_threads;
	stu_int_t  n, err;

	stu_worker_process_init(cycle, worker);

	if (stu_init_threads(threads_n, (size_t) -1) == STU_ERROR) {
		/* fatal */
		exit(2);
	}

	err = stu_thread_key_create(&stu_thread_key);
	if (err != STU_OK) {
		stu_log_error(err, "stu_thread_key_create failed");
		/* fatal */
		exit(2);
	}

	for (n = 0; n < threads_n; n++) {
		err = stu_cond_init(&stu_threads[n].cond);
		if (err != STU_OK) {
			stu_log_error(err, "stu_cond_init failed");
			/* fatal */
			exit(2);
		}

		if (stu_create_thread((stu_tid_t *) &stu_threads[n].id, stu_worker_thread_cycle, (void *) &stu_threads[n]) != 0) {
			/* fatal */
			exit(2);
		}
	}

	// main thread of sub process, wait for signal
	for ( ;; ) {
		sleep(5);
	}
}

static void
stu_worker_process_init(stu_cycle_t *cycle, stu_int_t worker) {
	stu_int_t  n;

	for (n = 0; n < stu_last_process; n++) {
		if (n == stu_process_slot) {
			continue;
		}

		if (stu_processes[n].pid == -1) {
			continue;
		}

		if (stu_processes[n].filedes[1] == -1) {
			continue;
		}

		if (close(stu_processes[n].filedes[1]) == -1) {
			stu_log_error(stu_errno, "close() filedes failed");
		}
	}

	if (close(stu_processes[stu_process_slot].filedes[0]) == -1) {
		stu_log_error(stu_errno, "close() filedes failed");
	}

	if (stu_filedes_add_event(stu_filedes, STU_READ_EVENT, stu_filedes_handler) == STU_ERROR) {
		/* fatal */
		exit(2);
	}
}

static void
stu_filedes_handler(stu_event_t *ev) {
	stu_int_t          n;
	stu_filedes_t      ch;
	stu_connection_t  *c;

	c = ev->data;
	stu_log_debug(0, "filedes handler");

	for ( ;; ) {
		n = stu_filedes_read(c->fd, &ch, sizeof(stu_filedes_t));
		stu_log_debug(0, "filedes: %i", n);

		if (n == STU_ERROR) {
			stu_epoll_del_event(ev, STU_READ_EVENT);
			stu_connection_close(c);
			return;
		}

		if (n == STU_AGAIN) {
			return;
		}

		stu_log_debug(0, "filedes command: %d", ch.command);

		switch (ch.command) {
		case STU_CMD_QUIT:
			stu_quit = 1;
			break;
		case STU_CMD_RESTART:
			stu_restart = 1;
			break;
		case STU_CMD_OPEN_FILEDES:
			stu_log_debug(0, "get filedes s:%i pid:%P fd:%d", ch.slot, ch.pid, ch.fd);

			stu_processes[ch.slot].pid = ch.pid;
			stu_processes[ch.slot].filedes[0] = ch.fd;
			break;
		case STU_CMD_CLOSE_FILEDES:
			stu_log_debug(0, "close filedes s:%i pid:%P our:%P fd:%d", ch.slot, ch.pid, stu_processes[ch.slot].pid, stu_processes[ch.slot].filedes[0]);

			if (close(stu_processes[ch.slot].filedes[0]) == -1) {
				stu_log_error(stu_errno, "close() filedes failed");
			}
			stu_processes[ch.slot].filedes[0] = -1;
			break;
		}
	}
}

static stu_thread_value_t
stu_worker_thread_cycle(void *data) {
	//stu_thread_t  *thr = data;

	// worker thread of sub process, handle connection/request
	for ( ;; ) {
		sleep(5);
	}

	return NULL;
}

