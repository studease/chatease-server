/*
 * stu_cycle.c
 *
 *  Created on: 2016-9-12
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"
#include <unistd.h>

stu_uint_t   stu_ncpu;
volatile stu_cycle_t *stu_cycle;

extern stu_hash_t *stu_upstreams;

static void stu_config_copy(stu_config_t *dst, stu_config_t *src, stu_pool_t *pool);


void
stu_config_default(stu_config_t *cf) {
	struct timeval  tv;
	stu_tm_t        tm;

	// log
	stu_memzero(&cf->log, sizeof(stu_file_t));

	stu_gettimeofday(&tv);
	stu_localtime(tv.tv_sec, &tm);

	cf->log.name.data = stu_calloc(STU_FILE_PATH_MAX_LEN);
	if (cf->log.name.data == NULL) {
		return;
	}

	stu_sprintf(cf->log.name.data, "logs/%4d-%02d-%02d %02d:%02d:%02d.log",
			tm.stu_tm_year, tm.stu_tm_mon, tm.stu_tm_mday,
			tm.stu_tm_hour, tm.stu_tm_min, tm.stu_tm_sec
		);
	cf->log.name.len = stu_strlen(cf->log.name.data);

	// pid
	stu_memzero(&cf->pid, sizeof(stu_file_t));
	stu_str_set(&cf->pid.name, "chatd.pid");

	cf->edition = PREVIEW; // ENTERPRISE
	cf->master_process = TRUE;
	cf->worker_processes = 1;
	cf->worker_threads = 4;

	cf->port = 80;
	stu_str_null(&cf->hostname);

	cf->push_users = TRUE;
	cf->push_users_interval = STU_CHANNEL_PUSH_USERS_DEFAULT_INTERVAL * 1000;

	cf->push_status = TRUE;
	cf->push_status_interval = STU_CHANNEL_PUSH_STATUS_DEFAULT_INTERVAL * 1000;
}

stu_cycle_t *
stu_cycle_create(stu_config_t *cf) {
	stu_pool_t            *pool;
	stu_slab_pool_t       *slab_pool;
	stu_connection_pool_t *connection_pool;
	stu_ram_pool_t        *ram_pool;
	stu_cycle_t           *cycle;
	stu_shm_t             *shm;

	stu_ncpu = sysconf(_SC_NPROCESSORS_ONLN);

	pool = stu_pool_create(STU_CYCLE_POOL_SIZE);
	if (pool == NULL) {
		stu_log_error(stu_errno, "Failed to create cycle pool.");
		return NULL;
	}

	cycle = stu_pcalloc(pool, sizeof(stu_cycle_t));
	if (cycle == NULL) {
		stu_log_error(0, "Failed to pcalloc cycle.");
		return NULL;
	}
	cycle->pool = pool;

	slab_pool = stu_slab_pool_create(STU_SLAB_POOL_DEFAULT_SIZE);
	if (slab_pool == NULL) {
		stu_log_error(stu_errno, "Failed to create slab pool.");
		return NULL;
	}
	cycle->slab_pool = slab_pool;

	connection_pool = stu_connection_pool_create(pool);
	if (connection_pool == NULL) {
		stu_log_error(0, "Failed to pcalloc connection pool.");
		return NULL;
	}
	cycle->connection_pool = connection_pool;

	ram_pool = stu_ram_pool_create();
	if (ram_pool == NULL) {
		stu_log_error(0, "Failed to create ram pool.");
		return NULL;
	}
	stu_queue_init(&cycle->ram_pool.queue);
	stu_queue_insert_tail(&cycle->ram_pool.queue, &ram_pool->queue);

	//stu_ram_test(&cycle->ram_pool);

	stu_config_copy(&cycle->config, cf, pool);

	// shared memory
	stu_list_init(&cycle->shared_memory, slab_pool, (stu_list_palloc_pt) stu_slab_alloc, (stu_list_free_pt) stu_slab_free);

	shm = stu_pcalloc(pool, sizeof(stu_shm_t));
	shm->addr = (u_char *) pool;
	shm->size = pool->data.end - (u_char *) pool;
	stu_list_push(&cycle->shared_memory, shm, sizeof(stu_shm_t));

	shm = stu_pcalloc(pool, sizeof(stu_shm_t));
	shm->addr = (u_char *) slab_pool;
	shm->size = slab_pool->data.end - (u_char *) slab_pool;
	stu_list_push(&cycle->shared_memory, shm, sizeof(stu_shm_t));

	shm = stu_pcalloc(pool, sizeof(stu_shm_t));
	shm->addr = (u_char *) ram_pool;
	shm->size = ram_pool->data.end - (u_char *) ram_pool;
	stu_list_push(&cycle->shared_memory, shm, sizeof(stu_shm_t));

	// channels
	if (stu_hash_init(&cycle->channels, NULL, STU_CHANNEL_MAXIMUM, slab_pool,
			(stu_hash_palloc_pt) stu_slab_alloc, (stu_hash_free_pt) stu_slab_free) == STU_ERROR) {
		stu_log_error(0, "Failed to init channel hash.");
		return NULL;
	}

	cycle->connection_n = 0;

	// timer
	if (stu_timer_init(cycle) == STU_ERROR) {
		stu_log_error(0, "Failed to init timer.");
		return NULL;
	}

	if (stu_hash_init(&cycle->timers, NULL, STU_TIMER_MAXIMUM, slab_pool,
			(stu_hash_palloc_pt) stu_slab_alloc, (stu_hash_free_pt) stu_slab_free) == STU_ERROR) {
		stu_log_error(0, "Failed to init timer hash.");
		return NULL;
	}

	// event
	if (stu_event_init() == STU_ERROR) {
		stu_log_error(0, "Failed to init event.");
		return NULL;
	}

	return cycle;
}

static void
stu_config_copy(stu_config_t *dst, stu_config_t *src, stu_pool_t *pool) {
	dst->log.name.data = stu_pcalloc(pool, src->log.name.len + 1);
	dst->log.name.len = src->log.name.len;
	memcpy(dst->log.name.data, src->log.name.data, src->log.name.len);

	dst->pid.name.data = stu_pcalloc(pool, src->pid.name.len + 1);
	dst->pid.name.len = src->pid.name.len;
	memcpy(dst->pid.name.data, src->pid.name.data, src->pid.name.len);

	dst->edition = src->edition;
	dst->master_process = src->master_process;
	dst->worker_processes = src->worker_processes;
	dst->worker_threads = src->worker_threads;

	dst->port = src->port;
	if (src->hostname.len > 0) {
		dst->hostname.data = stu_pcalloc(pool, src->hostname.len + 1);
		dst->hostname.len = src->hostname.len;
		memcpy(dst->hostname.data, src->hostname.data, src->hostname.len);
	}

	dst->push_users = src->push_users;
	dst->push_users_interval = src->push_users_interval;

	dst->push_status = src->push_status;
	dst->push_status_interval = src->push_status_interval;

	if (stu_hash_init(&dst->upstreams, NULL, STU_UPSTREAM_MAXIMUM, pool, (stu_hash_palloc_pt) stu_pcalloc, NULL) == STU_ERROR) {
		stu_log_error(0, "Failed to init upstream hash.");
		return;
	}

	stu_upstreams = &dst->upstreams;
}


stu_int_t
stu_pidfile_create(stu_file_t *pid) {
	u_char  temp[STU_FILE_PATH_MAX_LEN];

	pid->fd = stu_file_open(pid->name.data, STU_FILE_RDWR, STU_FILE_TRUNCATE, STU_FILE_DEFAULT_ACCESS);
	if (pid->fd == STU_FILE_INVALID) {
		stu_log_error(stu_errno, "Failed to " stu_file_open_n " pid file.");
		return STU_ERROR;
	}

	stu_memzero(temp, STU_FILE_PATH_MAX_LEN);
	stu_sprintf(temp, "%d", stu_getpid());

	if (stu_file_write(pid, temp, stu_strlen(temp), pid->offset) == STU_ERROR) {
		stu_log_error(stu_errno, "Failed to " stu_write_fd_n " pid file.");
		return STU_ERROR;
	}

	return STU_OK;
}

void
stu_pidfile_delete(stu_file_t *pid) {
	if (stu_file_delete(pid->name.data) == -1) {
		stu_log_error(stu_errno, "Failed to " stu_file_delete_n " pid file.");
	}
}

