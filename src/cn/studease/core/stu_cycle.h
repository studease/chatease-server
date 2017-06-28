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
	stu_file_t     log;
	stu_file_t     pid;

	stu_edition_t  edition;
	stu_bool_t     master_process;
	stu_int_t      worker_processes;
	stu_int_t      worker_threads;

	uint16_t       port;
	stu_str_t      hostname;

	stu_bool_t     push_users;
	stu_msec_t     push_users_interval;  // seconds

	stu_bool_t     push_status;
	stu_msec_t     push_status_interval; // seconds

	stu_hash_t     upstreams;            // => stu_list_t => stu_http_upstream_server_t
} stu_config_t;

struct stu_cycle_s {
	stu_pool_t            *pool;
	stu_slab_pool_t       *slab_pool;
	stu_connection_pool_t *connection_pool;
	stu_ram_pool_t         ram_pool;

	stu_config_t           config;
	stu_list_t             shared_memory; // => stu_shm_t

	stu_hash_t             channels;
	stu_uint_t             connection_n;

	stu_spinlock_t         timer_lock;
	stu_rbtree_t           timer_rbtree;
	stu_rbtree_node_t      timer_sentinel;
	stu_hash_t             timers;
};

void stu_config_default(stu_config_t *cf);

stu_cycle_t *stu_cycle_create(stu_config_t *cf);

stu_int_t stu_pidfile_create(stu_file_t *pid);
void stu_pidfile_delete(stu_file_t *pid);

stu_thread_key_t  stu_thread_key;

#endif /* STU_CYCLE_H_ */
