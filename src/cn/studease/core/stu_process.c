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

stu_process_t  stu_processes[STU_PROCESSES_MAXIMUM];
stu_int_t      stu_process_last;

sig_atomic_t   stu_quit;
sig_atomic_t   stu_restart;

stu_thread_t   stu_threads[STU_THREADS_MAXIMUM];
stu_int_t      stu_threads_n;

static void  stu_pass_open_filedes(stu_cycle_t *cycle, stu_filedes_t *fds);
static void  stu_signal_worker_processes(stu_cycle_t *cycle, int signo);
static void  stu_worker_process_cycle(stu_cycle_t *cycle, void *data);
static void  stu_worker_process_init(stu_cycle_t *cycle, stu_int_t worker);
static void  stu_filedes_handler(stu_event_t *ev);
static stu_thread_value_t stu_worker_thread_cycle(void *data);


void
stu_process_master_cycle(stu_cycle_t *cycle) {
	sigset_t  set;

	if (cycle->config.master_process == FALSE) {
		return;
	}

	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigaddset(&set, SIGALRM);
	sigaddset(&set, SIGIO);
	sigaddset(&set, SIGINT);
	sigaddset(&set, stu_signal_value(STU_SHUTDOWN_SIGNAL));
	sigaddset(&set, stu_signal_value(STU_CHANGEBIN_SIGNAL));

	if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
		stu_log_error(stu_errno, "sigprocmask() failed.");
	}

	sigemptyset(&set);

	for ( ;; ) {
		stu_log_debug(0, "sigsuspending...");
		sigsuspend(&set);

		if (stu_quit) {
			stu_signal_worker_processes(cycle, stu_signal_value(STU_SHUTDOWN_SIGNAL));
		}

		if (stu_restart) {
			stu_log("Restarting server...");
			// respawn processes
		}
	}
}

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
}

stu_pid_t
stu_spawn_process(stu_cycle_t *cycle, stu_spawn_proc_pt proc, void *data, char *name) {
	u_long     on;
	stu_pid_t  pid;
	stu_int_t  s;

	for (s = 0; s < stu_process_last; s++) {
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

	stu_log_debug(0, "filedes: %d, %d.", stu_processes[s].filedes[0], stu_processes[s].filedes[1]);

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

	stu_log_debug(0, "started \"%s\", pid: %d", name, pid);

	stu_processes[s].pid = pid;
	stu_processes[s].proc = proc;
	stu_processes[s].data = data;
	stu_processes[s].name = name;
	stu_processes[s].status = 0;
	stu_processes[s].state = 1;

	if (s == stu_process_last) {
		stu_process_last++;
	}

	return pid;
}

static void
stu_pass_open_filedes(stu_cycle_t *cycle, stu_filedes_t *fds) {
	stu_int_t  i;

	for (i = 0; i < stu_process_last; i++) {
		if (i == stu_process_slot || stu_processes[i].state == 0 || stu_processes[i].filedes[0] == 0) {
			continue;
		}

		stu_log_debug(0, "pass filedes: slot=%d, pid=%lu, fd=%d, to slot=%d, pid=%lu, fd=%d.",
				fds->slot, fds->pid, fds->fd, i, stu_processes[i].pid, stu_processes[i].filedes[0]);

		stu_filedes_write(stu_processes[i].filedes[0], fds, sizeof(stu_filedes_t));
	}
}

static void
stu_signal_worker_processes(stu_cycle_t *cycle, int signo) {
	stu_filedes_t  fds;
	stu_int_t      i;

	stu_memzero(&fds, sizeof(stu_channel_t));

	switch (signo) {
	case stu_signal_value(STU_SHUTDOWN_SIGNAL):
		fds.command = STU_CMD_QUIT;
		break;
	case stu_signal_value(STU_CHANGEBIN_SIGNAL):
		fds.command = STU_CMD_RESTART;
		break;
	default:
		fds.command = 0;
		break;
	}

	fds.fd = -1;

	for (i = 0; i < stu_process_last; i++) {
		stu_log_debug(0, "stu_processes[%d]: pid=%d, state=%d.", i, stu_processes[i].pid, stu_processes[i].state);

		if (stu_processes[i].pid == STU_INVALID_PID || stu_processes[i].state == 0) {
			continue;
		}

		if (fds.command) {
			if (stu_filedes_write(stu_processes[i].filedes[0], &fds, sizeof(stu_filedes_t)) == STU_OK) {
				continue;
			}
		}

		stu_log_debug(0, "kill (%P, %d)", stu_processes[i].pid, signo);

		if (kill(stu_processes[i].pid, signo) == -1) {
			stu_log_error(stu_errno, "kill(%d, %d) failed", stu_processes[i].pid, signo);
		}
	}
}

static void
stu_worker_process_cycle(stu_cycle_t *cycle, void *data) {
	stu_int_t  worker = (intptr_t) data;
	stu_int_t  threads_n = cycle->config.worker_threads;
	stu_int_t  n, err;

	stu_worker_process_init(cycle, worker);

	if (stu_init_threads(threads_n, STU_THREADS_DEFAULT_STACKSIZE) == STU_ERROR) {
		stu_log_error(0, "Failed to init threads.");
		exit(2);
	}

	err = stu_thread_key_create(&stu_thread_key);
	if (err != STU_OK) {
		stu_log_error(err, "stu_thread_key_create failed");
		exit(2);
	}

	for (n = 0; n < threads_n; n++) {
		if (stu_cond_init(&stu_threads[n].cond) == STU_ERROR) {
			stu_log_error(0, "stu_cond_init failed.");
			exit(2);
		}

		if (stu_create_thread((stu_tid_t *) &stu_threads[n].id, stu_worker_thread_cycle, (void *) &stu_threads[n]) == STU_ERROR) {
			stu_log_error(0, "Failed to create thread[%d].", n);
			exit(2);
		}
	}

	// main thread of sub process, wait for signal
	for ( ;; ) {
		if (stu_quit) {
			stu_log("Closing worker process...");
			break;
		}

		if (stu_restart) {
			stu_log("Restarting worker process...");
		}
	}
}

static void
stu_worker_process_init(stu_cycle_t *cycle, stu_int_t worker) {
	stu_int_t  n;

	for (n = 0; n < stu_process_last; n++) {
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
	//stu_log_debug(0, "filedes handler called.");

	for ( ;; ) {
		n = stu_filedes_read(c->fd, &ch, sizeof(stu_filedes_t));
		if (n == STU_ERROR) {
			stu_log_error(0, "Failed to read filedes message.");
			stu_epoll_del_event(ev, STU_READ_EVENT);
			stu_connection_close(c);
			return;
		}

		if (n == STU_AGAIN) {
			return;
		}

		stu_log_debug(0, "filedes command: %d.", ch.command);

		switch (ch.command) {
		case STU_CMD_QUIT:
			stu_quit = 1;
			break;
		case STU_CMD_RESTART:
			stu_restart = 1;
			break;
		case STU_CMD_OPEN_FILEDES:
			stu_log_debug(0, "open filedes: s=%i, pid=%d, fd=%d.", ch.slot, ch.pid, ch.fd);

			stu_processes[ch.slot].pid = ch.pid;
			stu_processes[ch.slot].filedes[0] = ch.fd;
			break;
		case STU_CMD_CLOSE_FILEDES:
			stu_log_debug(0, "close filedes: s=%i, pid=%d, our=%d, fd=%d.", ch.slot, ch.pid, stu_processes[ch.slot].pid, stu_processes[ch.slot].filedes[0]);

			if (close(stu_processes[ch.slot].filedes[0]) == -1) {
				stu_log_error(stu_errno, "close() filedes failed.");
			}
			stu_processes[ch.slot].filedes[0] = -1;
			break;
		}
	}
}

static stu_thread_value_t
stu_worker_thread_cycle(void *data) {
	//stu_thread_t     *thr = data;
	struct epoll_event  events[STU_EPOLL_EVENTS], *ev;
	stu_int_t           nev, i;
	stu_connection_t   *c;

	for ( ;; ) {
		nev = stu_epoll_process_events(events, STU_EPOLL_EVENTS, -1);
		if (nev <= 0) {
			stu_log_error(0, "epoll_wait error!");
			break;
		}

		for (i = 0; i < nev; i++) {
			ev = &events[i];
			c = (stu_connection_t *) events[i].data.ptr;

			if ((ev->events & EPOLLIN) && c->read->active) {
				c->read->handler(c->read);
			}

			if ((ev->events & EPOLLOUT) && c->write->active) {
				c->write->handler(c->write);
			}
		}
	}

	stu_log_error(0, "worker thread exit.");

	return NULL;
}

