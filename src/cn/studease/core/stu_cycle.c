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
	cf->daemon = TRUE;
	cf->edition = PREVIEW; // ENTERPRISE

	cf->port = 80;
	stu_str_null(&cf->hostname);

	cf->master_process = TRUE;
	cf->worker_processes = 1;
	cf->worker_threads = 4;

	cf->push_users = FALSE;
	cf->push_users_interval = STU_CHANNEL_PUSH_USERS_DEFAULT_INTERVAL;

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
	stu_queue_init(&cycle->ram_pool.queue);
	stu_queue_insert_tail(&cycle->ram_pool.queue, &ram_pool->queue);

	//stu_ram_test(&cycle->ram_pool);

	stu_config_copy(&cycle->config, cf, pool);
	stu_list_init(&cycle->shared_memory, slab_pool, (stu_list_palloc_pt) stu_slab_alloc, (stu_list_free_pt) stu_slab_free);

	shm = stu_pcalloc(pool, sizeof(stu_shm_t));
	shm->addr = (u_char *) pool;
	shm->size = pool->data.end - (u_char *) pool;
	/*if (stu_shm_alloc(shm) == STU_ERROR) {
		return NULL;
	}*/
	stu_list_push(&cycle->shared_memory, shm, sizeof(stu_shm_t));

	shm = stu_pcalloc(pool, sizeof(stu_shm_t));
	shm->addr = (u_char *) slab_pool;
	shm->size = slab_pool->data.end - (u_char *) slab_pool;
	/*if (stu_shm_alloc(shm) == STU_ERROR) {
		return NULL;
	}*/
	stu_list_push(&cycle->shared_memory, shm, sizeof(stu_shm_t));

	shm = stu_pcalloc(pool, sizeof(stu_shm_t));
	shm->addr = (u_char *) ram_pool;
	shm->size = ram_pool->data.end - (u_char *) ram_pool;
	/*if (stu_shm_alloc(shm) == STU_ERROR) {
		return NULL;
	}*/
	stu_list_push(&cycle->shared_memory, shm, sizeof(stu_shm_t));

	if (stu_hash_init(&cycle->channels, NULL, STU_MAX_CHANNEL_N, slab_pool,
			(stu_hash_palloc_pt) stu_slab_alloc, (stu_hash_free_pt) stu_slab_free) == STU_ERROR) {
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
	stu_list_t            *upstream;
	stu_upstream_server_t *server;

	dst->daemon = src->daemon;
	dst->edition = src->edition;

	dst->port = src->port;
	if (src->hostname.len > 0) {
		dst->hostname.data = stu_pcalloc(pool, src->hostname.len + 1);
		dst->hostname.len = src->hostname.len;
		memcpy(dst->hostname.data, src->hostname.data, src->hostname.len);
	}

	dst->master_process = src->master_process;
	dst->worker_processes = src->worker_processes;
	dst->worker_threads = src->worker_threads;

	dst->push_users = src->push_users;
	dst->push_users_interval = src->push_users_interval;

	dst->pid.data = stu_pcalloc(pool, src->pid.len + 1);
	dst->pid.len = src->pid.len;
	memcpy(dst->pid.data, src->pid.data, src->pid.len);

	// init upstream
	upstream = stu_pcalloc(pool, sizeof(stu_list_t));
	if (upstream == NULL) {
		stu_log_error(0, "Failed to pcalloc upstream list.");
		return;
	}

	stu_list_init(upstream, pool, (stu_list_palloc_pt) stu_pcalloc, NULL);

	// add server
	server = stu_pcalloc(pool, sizeof(stu_upstream_server_t));
	if (server == NULL) {
		stu_log_error(0, "Failed to pcalloc upstream server.");
		return;
	}

	server->name.data = stu_pcalloc(pool, 6);
	server->name.len = 5;
	memcpy(server->name.data, "ident", 5);

	server->port = 80;
	server->weight = 32;

	server->addr.name.data = stu_pcalloc(pool, 14);
	server->addr.name.len = 13;
	memcpy(server->addr.name.data, "192.168.4.247", 13);

	server->addr.sockaddr.sin_family = AF_INET;
	server->addr.sockaddr.sin_addr.s_addr = inet_addr((const char *) server->addr.name.data);
	server->addr.sockaddr.sin_port = htons(server->port);
	bzero(&(server->addr.sockaddr.sin_zero), 8);
	server->addr.socklen = sizeof(struct sockaddr);

	stu_list_push(upstream, server, sizeof(stu_upstream_server_t));

	// init upstreams
	if (stu_hash_init(&dst->upstreams, NULL, STU_UPSTREAM_MAXIMUM, pool, (stu_hash_palloc_pt) stu_pcalloc, NULL) == STU_ERROR) {
		stu_log_error(0, "Failed to init upstream hash.");
		return;
	}

	if (stu_hash_insert(&dst->upstreams, &server->name, upstream, STU_HASH_LOWCASE|STU_HASH_REPLACE) == STU_ERROR) {
		stu_log_error(0, "Failed to insert upstream %s into hash.", server->name.data);
		return;
	}

	stu_upstreams = &dst->upstreams;
}

