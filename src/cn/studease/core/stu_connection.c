/*
 * stu_connection.c
 *
 *  Created on: 2016-9-26
 *      Author: Tony Lau
 */

#include <unistd.h>
#include <sys/socket.h>
#include "stu_config.h"
#include "stu_core.h"

extern stu_cycle_t *stu_cycle;


static void stu_connection_init(stu_connection_t *c, stu_socket_t s);

static stu_connection_page_t *stu_connection_page_create(stu_connection_pool_t *pool);
static void stu_connection_page_init(stu_connection_page_t *page);
static stu_connection_t *stu_connection_page_alloc(stu_connection_page_t *page);


stu_connection_pool_t *
stu_connection_pool_create(stu_pool_t *pool) {
	stu_connection_pool_t *p;

	p = stu_pcalloc(pool, sizeof(stu_connection_pool_t));
	if (p == NULL) {
		return NULL;
	}

	stu_spinlock_init(&p->lock);
	stu_connection_page_init(&p->pages);

	return p;
}

stu_connection_t *
stu_connection_get(stu_socket_t s) {
	stu_connection_pool_t *pool;
	stu_connection_page_t *pages, *p, *last;
	stu_queue_t           *q;
	stu_connection_t      *c = NULL;

	pool = stu_cycle->connection_pool;
	pages = &pool->pages;
	for (q = stu_queue_head(&pages->queue); q != stu_queue_sentinel(&pages->queue); q = stu_queue_next(q)) {
		p = stu_queue_data(q, stu_connection_page_t, queue);
		if (p->length < STU_CONNECTIONS_PER_PAGE) {
			stu_spin_lock(&p->lock);

			c = stu_connection_page_alloc(p);
			if (c == NULL) {
				stu_spin_unlock(&p->lock);
				continue;
			}
			stu_connection_init(c, s);

			stu_spin_unlock(&p->lock);
			break;
		}
	}

	if (c == NULL) {
		stu_spin_lock(&pool->lock);

		p = stu_connection_page_create(pool);
		if (p == NULL) {
			stu_log_error(0, "Failed to create connection page.");
			stu_spin_unlock(&pool->lock);
			return NULL;
		}

		c = stu_connection_page_alloc(p); // no need to lock p
		stu_connection_init(c, s);

		q = stu_queue_last(&pool->pages.queue);
		last = stu_queue_data(q, stu_connection_page_t, queue);

		stu_spin_lock(&last->lock);
		stu_queue_insert_after(&last->queue, &p->queue);
		stu_spin_unlock(&last->lock);

		pool->pages.length ++;

		stu_spin_unlock(&pool->lock);
	}

	c->pool = stu_ram_alloc(stu_cycle->ram_pool);

	stu_spinlock_init(&c->pool->lock);
	c->pool->data.start = c->pool->data.last = (u_char *) c->pool + sizeof(stu_base_pool_t);
	c->pool->data.end = (u_char *) c->pool + STU_RAM_BLOCK_SIZE;
	c->pool->prev = c->pool->next = NULL;

	c->read = (stu_event_t *) c->pool->data.last;
	c->pool->data.last += sizeof(stu_event_t);
	c->read->active = FALSE;
	c->read->data = (void *) c;

	c->write = (stu_event_t *) c->pool->data.last;
	c->pool->data.last += sizeof(stu_event_t);
	c->write->active = TRUE;
	c->write->data = (void *) c;

	return c;
}

void
stu_connection_free(stu_connection_t *c) {
	stu_spin_lock(&c->page->lock);

	stu_ram_free(stu_cycle->ram_pool, (void *) c->pool);

	c->fd = (stu_socket_t) -1;
	c->read = c->write = NULL;

	stu_queue_remove(&c->queue);
	stu_queue_insert_tail(&c->page->free.queue, &c->queue);

	stu_spin_unlock(&c->page->lock);
}

void
stu_connection_close(stu_connection_t *c) {
	if (c->fd == (stu_socket_t) -1) {
		stu_log_debug(0, "connection already closed.");
		return;
	}

	stu_close_socket(c->fd);
	stu_connection_free(c);
}


static void
stu_connection_init(stu_connection_t *c, stu_socket_t s) {
	stu_queue_init(&c->queue);
	stu_spinlock_init(&c->lock);

	c->pool = NULL;

	c->fd = s;
	stu_user_init(&c->user);

	c->read = c->write = NULL;

	c->error = STU_CONNECTION_ERROR_NONE;
}

static stu_connection_page_t *
stu_connection_page_create(stu_connection_pool_t *pool) {
	size_t                 size;
	stu_connection_page_t *page;
	stu_shm_t             *shm = NULL;

	if (pool->pages.length >= STU_CONNECTION_MAX_PAGE) {
		stu_log_error(0, "Failed to alloc connection page: Page length limited.");
		goto failed;
	}

	size = sizeof(stu_connection_page_t) + STU_CONNECTIONS_PER_PAGE * sizeof(stu_connection_t);
	page = stu_alloc(size);
	if (page == NULL) {
		stu_log_error(stu_errno, "Failed to alloc connection page.");
		goto failed;
	}

	shm = stu_slab_calloc(stu_cycle->slab_pool, sizeof(stu_shm_t));
	shm->addr = (u_char *) page;
	shm->size = size;
	if (stu_shm_alloc(shm) == STU_ERROR) {
		stu_log_error(0, "Failed to alloc shm for connection page.");
		goto failed;
	}
	stu_list_push(&stu_cycle->shared_memory, (void *) shm, sizeof(stu_shm_t));

	stu_connection_page_init(page);
	page->data.start = page->data.last = (u_char *) page + sizeof(stu_connection_page_t);
	page->data.end = (u_char *) page + size;

	goto done;

failed:

	if (page != NULL) {
		stu_free(page);
	}
	if (shm != NULL) {
		stu_slab_free(stu_cycle->slab_pool, shm);
	}
	page = NULL;

done:

	return page;
}

static void
stu_connection_page_init(stu_connection_page_t *page) {
	stu_queue_init(&page->queue);
	stu_spinlock_init(&page->lock);

	page->data.start = page->data.last = page->data.end = NULL;

	stu_connection_init(&page->connections, 0);
	stu_connection_init(&page->free, 0);

	page->length = 0;
}

static stu_connection_t *
stu_connection_page_alloc(stu_connection_page_t *page) {
	stu_queue_t      *q;
	stu_connection_t *c;
	size_t            size;

	if (page->length == STU_CONNECTIONS_PER_PAGE) {
		return NULL;
	}

	q = stu_queue_head(&page->free.queue);
	if (q !=  stu_queue_sentinel(&page->free.queue)) {
		c = stu_queue_data(q, stu_connection_t, queue);
		stu_queue_remove(&c->queue);
		goto done;
	}

	size = sizeof(stu_connection_t);
	if (page->data.end - page->data.last < size) {
		return NULL;
	}

	c = (stu_connection_t *) page->data.last;
	page->data.last += size;

done:

	c->page = page;
	stu_queue_insert_tail(&page->connections.queue, &c->queue);
	page->length ++;

	return c;
}

