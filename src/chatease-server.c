/*
 ============================================================================
 Name        : chatease-server.c
 Author      : Tony Lau
 Version     :
 Copyright   : studease.cn
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include "cn/studease/core/stu_config.h"
#include "cn/studease/core/stu_core.h"
#include <stdio.h>
#include <stdlib.h>

extern stu_cycle_t *stu_cycle;


int main(int argc, char **argv) {
	int           arg;
	char          val;
	stu_config_t  cf;

	stu_log("GCC %s", __VERSION__);
	stu_log(__NAME "/" __VERSION);

	stu_config_default(&cf);
	while ((arg = getopt(argc, argv, ":m:p:w:t:c:")) != -1) {
		switch (arg) {
		case 'e':
			cf.edition = stu_utils_get_edition((u_char *) optarg, strlen(optarg));
			stu_log_debug(0, "Edition: %s", optarg);
			break;
		case 'p':
			cf.port = atoi(optarg);
			stu_log_debug(0, "Port: %s", optarg);
			break;
		case 'w':
			cf.worker_processes = atoi(optarg);
			stu_log_debug(0, "Worker_processes: %s", optarg);
			break;
		case 't':
			cf.worker_threads = atoi(optarg);
			stu_log_debug(0, "Worker_threads: %s", optarg);
			break;
		case '?':
			val = optopt;
			stu_log_error(0, "Illegal argument \"%d\", ignored.", val);
			break;
		case ':':
			stu_log_error(0, "Required parameter not found!");
			break;
		}
	}

	stu_cycle = stu_cycle_create(&cf);
	if (stu_cycle == NULL) {
		stu_log_error(0, "Failed to init cycle.");
		return EXIT_FAILURE;
	}

	if (stu_pidfile_create(&stu_cycle->config.pid) == STU_ERROR) {
		stu_log_error(0, "Failed to create pid file.");
		return EXIT_FAILURE;
	}

	if (stu_http_add_listen(&stu_cycle->config) == STU_ERROR) {
		stu_log_error(0, "Failed to add websocket listen.");
		return EXIT_FAILURE;
	}

	stu_start_worker_processes(stu_cycle);
	stu_process_master_cycle(stu_cycle);

	return EXIT_SUCCESS;
}
