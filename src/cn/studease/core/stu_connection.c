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
#include "stu_event.h"

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
	stu_connection_page_t *pages, *p;
	stu_queue_t           *q;
	stu_uint_t             lock;
	stu_connection_t      *c;

	pool = stu_cycle->connection_pool;
	pages = &pool->pages;
	c = NULL;

start:

	for (q = stu_queue_head(&pages->queue); q != stu_queue_sentinel(&pages->queue); q = stu_queue_next(q)) {
		p = stu_queue_data(q, stu_connection_page_t, queue);
		if (stu_atomic_fetch(&p->length) < STU_CONNECTIONS_PER_PAGE) {
			lock = stu_atomic_fetch(&p->lock.rlock.counter);
			if ((lock >> 16) !=  (lock & STU_SPINLOCK_OWNER_MASK)) {
				continue;
			}

			stu_spin_lock(&p->lock);

			c = stu_connection_page_alloc(p);
			if (c == NULL) {
				stu_spin_unlock(&p->lock);
				continue;
			}

			stu_spin_unlock(&p->lock);

			goto done;
		}
	}

	if (c == NULL) {
		stu_spin_lock(&pool->lock);

		p = stu_connection_page_create(pool);
		if (p == NULL) {
			stu_spin_unlock(&pool->lock);
			stu_log_error(0, "Failed to create connection page.");

			return NULL;
		}

		stu_queue_insert_tail(&pages->queue, &p->queue);

		stu_spin_unlock(&pool->lock);

		goto start;
	}

done:

	stu_atomic_fetch_add(&stu_cycle->connection_n, 1);

	c->pool = stu_ram_alloc(&stu_cycle->ram_pool);

	stu_spinlock_init(&c->pool->lock);
	c->pool->data.start = c->pool->data.last = (u_char *) c->pool + sizeof(stu_base_pool_t);
	c->pool->data.end = (u_char *) c->pool + STU_RAM_BLOCK_SIZE;
	c->pool->prev = c->pool->next = NULL;

	stu_connection_init(c, s);

	c->read.data = c->write.data = (void *) c;
	c->read.active = 0;
	c->write.active = 1;

	stu_log_debug(2, "Got connection: c=%p, fd=%d.", c, c->fd);

	return c;
}

void
stu_connection_free(stu_connection_t *c) {
	stu_socket_t  fd;

	stu_spin_lock(&c->page->lock);

	fd = c->fd;
	c->fd = (stu_socket_t) -1;
	c->read.active = c->write.active = 0;

	c->queue.next->prev = c->queue.prev;
	c->queue.prev->next = c->queue.next;
	stu_queue_insert_tail(&c->page->free.queue, &c->queue);

	c->page->length--;
	stu_atomic_fetch_sub(&stu_cycle->connection_n, 1);

	stu_ram_free(&stu_cycle->ram_pool, (void *) c->pool);

	stu_log_debug(2, "Freed connection: c=%p, fd=%d.", c, fd);

	stu_spin_unlock(&c->page->lock);
}

void
stu_connection_close(stu_connection_t *c) {
	/*if (c->fd == (stu_socket_t) -1) {
		stu_log_debug(2, "connection already closed.");
		return;
	}*/

	stu_close_socket(c->fd);
	stu_connection_free(c);
}


static void
stu_connection_init(stu_connection_t *c, stu_socket_t s) {
	stu_spinlock_init(&c->lock);

	c->fd = s;

	stu_user_init(&c->user, NULL, NULL, c->pool);

	c->buffer.start = c->buffer.last = c->buffer.end = NULL;
	c->data = NULL;
	c->read.active = c->write.active = 0;

	c->upstream = NULL;

	c->error = STU_CONNECTION_ERROR_NONE;
}

static stu_connection_page_t *
stu_connection_page_create(stu_connection_pool_t *pool) {
	size_t                 size;
	stu_connection_page_t *page;
	stu_shm_t             *shm;

	page = NULL;
	shm = NULL;

	if (pool->pages.length >= STU_CONNECTION_PAGE_MAX_N) {
		stu_log_error(0, "Failed to alloc connection page: Page length limited.");
		goto failed;
	}

	size = sizeof(stu_connection_page_t) + STU_CONNECTIONS_PER_PAGE * sizeof(stu_connection_t);

	shm = stu_pcalloc(stu_cycle->pool, sizeof(stu_shm_t));
	shm->size = size;
	if (stu_shm_alloc(shm) == STU_ERROR) {
		stu_log_error(0, "Failed to alloc alloc connection page.");
		goto failed;
	}

	stu_list_push(&stu_cycle->shared_memory, shm, sizeof(stu_shm_t));

	page = (stu_connection_page_t *) shm->addr;

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

	pool->pages.length++;

	return page;
}

static void
stu_connection_page_init(stu_connection_page_t *page) {
	stu_queue_init(&page->queue);
	stu_spinlock_init(&page->lock);

	page->data.start = page->data.last = page->data.end = NULL;

	stu_queue_init(&page->connections.queue);
	stu_queue_init(&page->free.queue);

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
	if (q != stu_queue_sentinel(&page->free.queue)) {
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
	page->length++;

	return c;
}

