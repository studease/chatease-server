/*
 * stu_cycle.h
 *
 *  Created on: 2016-9-9
 *      Author: Tony Lau
 */

#ifndef STU_CYCLE_H_
#define STU_CYCLE_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_CYCLE_POOL_SIZE  STU_POOL_DEFAULT_SIZE

typedef struct {
	stu_bool_t     daemon;
	stu_runmode_t  runmode;

	uint16_t       port;
	stu_str_t      hostname;

	stu_str_t      origin_addr;
	uint16_t       origin_port;

	stu_int_t      worker_processes;
	stu_int_t      worker_threads;

	stu_str_t      pid;
} stu_config_t;

struct stu_cycle_s {
	stu_pool_t            *pool;
	stu_slab_pool_t       *slab_pool;
	stu_connection_pool_t *connection_pool;

	stu_config_t           config;
	stu_list_t             shared_memory;   //type: stu_shm_t

	stu_hash_t             channels;
	stu_uint_t             connection_n;
};

void stu_config_default(stu_config_t *cf);

stu_cycle_t *stu_cycle_create(stu_config_t *cf);

stu_int_t stu_pidfile_create(stu_str_t *name);
void stu_pidfile_delete(stu_str_t *name);

stu_thread_key_t  stu_thread_key;

#endif /* STU_CYCLE_H_ */
