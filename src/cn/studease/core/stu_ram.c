/*
 * stu_ram.c
 *
 *  Created on: 2016-10-31
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static stu_ram_page_t *stu_ram_alloc_page(stu_ram_pool_t *pool);
static void stu_ram_free_page(stu_ram_pool_t *pool, stu_ram_page_t *page);


stu_ram_pool_t *
stu_ram_pool_create() {
	stu_ram_pool_t *p;
	stu_uint_t      size, n;
	u_char         *c;

	size = 1024 + 8 * STU_RAM_PAGE_SIZE;
	p = stu_alloc(size);
	if (p == NULL) {
		return NULL;
	}

	stu_spinlock_init(&p->lock);
	p->data.end = (u_char *) p + size;

	c = (u_char *) p + sizeof(stu_ram_pool_t);

	p->slots = (stu_ram_page_t *) c;
	p->slots->bitmap = 0;
	p->slots->prev = p->slots->next = p->slots;

	c += sizeof(stu_ram_page_t);

	stu_memzero(c, 8 * sizeof(stu_ram_page_t));
	p->pages = (stu_ram_page_t *) c;

	c += 8 * sizeof(stu_ram_page_t);

	p->last = (stu_ram_page_t *) c;
	p->free.prev = p->free.next = &p->free;

	//stu_memzero(c, 512);
	for (n = 0; n < 8; n++) {
		p->pages[n].bitmap = (uintptr_t) c;
		c += 64;
	}

	p->data.start = p->data.last = (u_char *) stu_align_ptr((uintptr_t) c, STU_RAM_BLOCK_SIZE);

	stu_log_debug(1, "created new ram pool %p.", p);

	return p;
}

void *
stu_ram_alloc(stu_ram_pool_t *pool) {
	stu_ram_pool_t *t;
	stu_queue_t    *q;
	stu_uint_t      lock;
	void           *p;

	t = NULL;
	p = NULL;

start:

	for (q = stu_queue_head(&pool->queue); q != stu_queue_sentinel(&pool->queue); q = stu_queue_next(q)) {
		t = stu_queue_data(q, stu_ram_pool_t, queue);
		if (t->bitmap != STU_RAM_BUSY64) {
			lock = stu_atomic_read(&t->lock.rlock.counter);
			if ((lock >> 16) !=  (lock & STU_SPINLOCK_OWNER_MASK)) {
				continue;
			}

			stu_spin_lock(&t->lock);

			p = stu_ram_alloc_locked(t);
			if (p == NULL) {
				stu_spin_unlock(&t->lock);
				continue;
			}

			stu_spin_unlock(&t->lock);

			goto done;
		}
	}

	if (pool->bitmap < STU_RAM_POOL_MAX_N) {
		stu_spin_lock(&pool->lock);

		t = stu_ram_pool_create();
		if (t == NULL) {
			stu_spin_unlock(&pool->lock);
			stu_log_error(0, "Failed to create ram pool.");

			return NULL;
		}

		stu_queue_insert_tail(&pool->queue, &t->queue);

		stu_spin_unlock(&pool->lock);

		goto start;
	}

done:

	return p;
}

void *
stu_ram_alloc_locked(stu_ram_pool_t *pool) {
	stu_ram_page_t *page, *sentinel;
	uint64_t       *bitmap;
	uint8_t         i, j, k, m;
	u_char         *c, *p, *b;

start:

	sentinel = pool->slots;

	for (page = sentinel->next; page != sentinel; page = page->next) {
		bitmap = (uint64_t *) page->bitmap;

		for (i = 0; i < 8; i++, bitmap++) {
			if (*bitmap == STU_RAM_BUSY64) {
				continue;
			}

			c = (u_char *) bitmap;

			for (j = 0; j < 8; j++, c++) {
				if (*c == STU_RAM_BUSY8) {
					continue;
				}

				for (m = 1, k = 0; *c & m; m <<= 1, k++) {
					/* void */
				}

				*c |= m;

				p = pool->data.start;
				p += (page - pool->pages) << STU_RAM_PAGE_SHIFT;
				p += (i * 64 + j * 8 + k) << STU_RAM_BLOCK_SHIFT;

				if (*bitmap == STU_RAM_BUSY64) {
					b = (u_char *) &pool->bitmap + (page - pool->pages);
					*b &= ~(1 << i);

					bitmap++;

					for (i = i + 1; i < 8; i++, bitmap++) {
						if (*bitmap != STU_RAM_BUSY64) {
							goto done;
						}
					}

					goto full;
				}

				goto done;
			}
		}
	}

	page = stu_ram_alloc_page(pool);
	if (page == NULL) {
		return NULL;
	}

	stu_memzero((void *) page->bitmap, 64);

	page->prev = sentinel->prev;
	page->prev->next = page;
	page->next = sentinel;
	sentinel->prev = page;

	goto start;

full:

	page->prev->next = page->next;
	page->next->prev = page->prev;
	page->prev = page->next = NULL;

	stu_log_debug(1, "ram page full %p.", page);

done:

	stu_log_debug(1, "ram alloc: %p", p);

	return p;
}

void
stu_ram_free(stu_ram_pool_t *pool, void *p) {
	stu_ram_pool_t *t;
	stu_queue_t    *q;

	for (q = stu_queue_head(&pool->queue); q != stu_queue_sentinel(&pool->queue); q = stu_queue_next(q)) {
		t = stu_queue_data(q, stu_ram_pool_t, queue);
		if ((u_char *) p < t->data.start || (u_char *) p > t->data.end) {
			continue;
		}

		stu_spin_lock(&t->lock);
		stu_ram_free_locked(t, p);
		stu_spin_unlock(&t->lock);

		break;
	}
}

void
stu_ram_free_locked(stu_ram_pool_t *pool, void *p) {
	stu_ram_page_t *page, *sentinel;
	stu_uint_t      x, i, j, k, m, n;
	uint64_t       *bitmap;
	u_char         *c, *b;

	if ((u_char *) p < pool->data.start || (u_char *) p > pool->data.end) {
		stu_log_error(0, "Failed to free ram: outside of pool.");
		return;
	}

	x = ((u_char *) p - pool->data.start) >> STU_RAM_PAGE_SHIFT;
	page = &pool->pages[x];
	sentinel = pool->slots;

	k = ((u_char *) p - pool->data.start - (x << STU_RAM_PAGE_SHIFT)) >> STU_RAM_BLOCK_SHIFT;
	j = k >> 3;
	i = j >> 3;

	bitmap = (uint64_t *) page->bitmap;
	c = (u_char *) bitmap + j;

	m = 1 << (k % 8);
	*c &= ~m;

	b = (u_char *) &pool->bitmap + x;
	*b &= ~(1 << i);

	if (page->prev == NULL) {
		goto append;
	}

	n = 8;
	while (n--) {
		if (*bitmap) {
			goto done;
		}
		bitmap++;
	}
	stu_ram_free_page(pool, page);

	goto done;

append:

	page->prev = sentinel->prev;
	page->next = sentinel;
	sentinel->prev->next = page;
	sentinel->prev = page;

done:

	stu_log_debug(1, "ram free: %p", p);
}


static stu_ram_page_t *
stu_ram_alloc_page(stu_ram_pool_t *pool) {
	stu_ram_page_t *page;
	stu_uint_t      n;

	page = pool->free.next;
	if (page != &pool->free) {
		pool->free.next = page->next;
		page->next->prev = &pool->free;

		stu_log_debug(1, "got freed ram page %p.", page);

		return page;
	}

	n = (pool->data.last - pool->data.start) >> STU_RAM_PAGE_SHIFT;
	page = &pool->pages[n];
	if (page >= pool->last) {
		stu_log_debug(1, "Failed to alloc ram page: no memory.");
		return NULL;
	}

	pool->data.last += STU_RAM_PAGE_SIZE;

	stu_log_debug(1, "got new ram page %p.", page);

	return page;
}

static void
stu_ram_free_page(stu_ram_pool_t *pool, stu_ram_page_t *page) {
	if (page->prev != NULL && page->next != NULL) {
		page->prev->next = page->next;
		page->next->prev = page->prev;
	}

	page->prev = pool->free.prev;
	page->next = &pool->free;
	pool->free.prev->next = page;
	pool->free.prev = page;

	stu_log_debug(1, "freed ram page %p.", page);
}


void
stu_ram_test(stu_ram_pool_t *pool) {
	stu_int_t  n;
	void      *a[513];
	void      *p;

	stu_log_debug(1, "ram alloc test starting...");

	for (n = 0; n <= 512; n++) {
		p = stu_ram_alloc(pool);
		a[n] = p;
	}

	stu_log_debug(1, "ram free test starting...");

	for (n = 0; n <= 512; n++) {
		p = a[n];
		stu_ram_free(pool, p);
	}

	stu_log_debug(1, "ram alloc test starting...");

	for (n = 0; n <= 512; n++) {
		p = stu_ram_alloc(pool);
		a[n] = p;
	}

	stu_log_debug(1, "ram free test starting...");

	for (n = 512; n >= 0; n--) {
		p = a[n];
		stu_ram_free(pool, p);
	}

	stu_log_debug(1, "ram test finished.");
}

