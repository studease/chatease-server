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
stu_cycle_t *stu_cycle;


static void stu_config_copy(stu_config_t *dst, stu_config_t *src, stu_pool_t *pool);


void
stu_config_default(stu_config_t *cf) {
	cf->daemon = TRUE;
	cf->runmode = STANDALONE;
	cf->port = 80;
	stu_str_null(&cf->hostname);
	stu_str_null(&cf->origin_addr);
	cf->origin_port = 80;
	cf->master_process = TRUE;
	cf->worker_processes = 1;
	cf->worker_threads = 1;
	stu_str_set(&cf->pid, "chatd.pid");
}

stu_cycle_t *
stu_cycle_create(stu_config_t *cf) {
	stu_pool_t            *pool;
	stu_slab_pool_t       *slab_pool;
	stu_connection_pool_t *connection_pool;
	stu_ram_pool_t        *ram_pool;
	stu_cycle_t           *cycle;
	stu_shm_t             *shm;
	stu_list_elt_t        *elt;

	stu_strerror_init();

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
	cycle->ram_pool = ram_pool;

	stu_config_copy(&cycle->config, cf, pool);
	stu_list_init(&cycle->shared_memory, slab_pool);

	shm = stu_pcalloc(pool, sizeof(stu_shm_t));
	shm->addr = (u_char *) pool;
	shm->size = pool->data.end - (u_char *) pool;
	if (stu_shm_alloc(shm) == STU_ERROR) {
		return NULL;
	}
	elt = stu_pcalloc(pool, sizeof(stu_list_elt_t));
	elt->obj = (void *) shm;
	elt->size = sizeof(stu_shm_t);
	stu_list_push(&cycle->shared_memory, elt);

	shm = stu_pcalloc(pool, sizeof(stu_shm_t));
	shm->addr = (u_char *) slab_pool;
	shm->size = slab_pool->data.end - (u_char *) slab_pool;
	if (stu_shm_alloc(shm) == STU_ERROR) {
		return NULL;
	}
	elt = stu_pcalloc(pool, sizeof(stu_list_elt_t));
	elt->obj = (void *) shm;
	elt->size = sizeof(stu_shm_t);
	stu_list_push(&cycle->shared_memory, elt);

	shm = stu_pcalloc(pool, sizeof(stu_shm_t));
	shm->addr = (u_char *) ram_pool;
	shm->size = ram_pool->data.end - (u_char *) ram_pool;
	if (stu_shm_alloc(shm) == STU_ERROR) {
		return NULL;
	}
	elt = stu_pcalloc(pool, sizeof(stu_list_elt_t));
	elt->obj = (void *) shm;
	elt->size = sizeof(stu_shm_t);
	stu_list_push(&cycle->shared_memory, elt);

	if (stu_hash_init(&cycle->channels, STU_MAX_CHANNEL_N, slab_pool) == STU_ERROR) {
		stu_log_error(0, "Failed to init channels hash.");
		return NULL;
	}

	cycle->connection_n = 0;

	if (stu_epoll_init() == STU_ERROR) {
		stu_log_error(0, "Failed to init epoll.");
		return NULL;
	}

	return cycle;
}

stu_int_t
stu_pidfile_create(stu_str_t *name) {
	return STU_OK;
}

void
stu_pidfile_delete(stu_str_t *name) {

}


static void
stu_config_copy(stu_config_t *dst, stu_config_t *src, stu_pool_t *pool) {
	dst->daemon = src->daemon;
	dst->runmode = src->runmode;
	dst->port = src->port;

	if (src->hostname.len > 0) {
		dst->hostname.data = stu_pcalloc(pool, src->hostname.len + 1);
		memcpy(dst->hostname.data, src->hostname.data, src->hostname.len);
		dst->hostname.len = src->hostname.len;
	}

	if (src->runmode == ORIGIN) {
		dst->origin_addr.data = stu_pcalloc(pool, src->origin_addr.len + 1);
		memcpy(dst->origin_addr.data, src->origin_addr.data, src->origin_addr.len);
		dst->origin_addr.len = src->origin_addr.len;

		dst->origin_port = src->origin_port;
	}

	dst->master_process = src->master_process;
	dst->worker_processes = src->worker_processes;
	dst->worker_threads = src->worker_threads;

	dst->pid.data = stu_pcalloc(pool, src->pid.len + 1);
	memcpy(dst->pid.data, src->pid.data, src->pid.len);
	dst->pid.len = src->pid.len;
}

