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

	return p;
}

void *
stu_ram_alloc(stu_ram_pool_t *pool) {
	void *p;

	stu_spin_lock(&pool->lock);
	p = stu_ram_alloc_locked(pool);
	stu_spin_unlock(&pool->lock);

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

	page = sentinel->next;
	if (page == sentinel) {
		page = stu_ram_alloc_page(pool);
		if (page == NULL) {
			return NULL;
		}

		stu_memzero((void *) page->bitmap, 64);

		page->prev = page->next = sentinel;
		sentinel->prev = sentinel->next = page;
	}

	goto start;

full:

	page->prev->next = page->next;
	page->next->prev = page->prev;
	page->prev = page->next = NULL;

done:

	stu_log_debug(0, "ram alloc: %p", p);

	return p;
}

void
stu_ram_free(stu_ram_pool_t *pool, void *p) {
	stu_spin_lock(&pool->lock);
	stu_ram_free_locked(pool, p);
	stu_spin_unlock(&pool->lock);
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

append:

	page->prev = sentinel->prev;
	page->next = sentinel;
	sentinel->prev->next = page;
	sentinel->prev = page;

done:

	stu_log_debug(0, "ram free: %p", p);
}


static stu_ram_page_t *
stu_ram_alloc_page(stu_ram_pool_t *pool) {
	stu_ram_page_t *page;
	stu_uint_t      n;

	page = pool->free.next;
	if (page != &pool->free) {
		pool->free.next = page->next;
		page->next->prev = &pool->free;
		return page;
	}

	n = (pool->data.last - pool->data.start) >> STU_RAM_PAGE_SHIFT;
	page = &pool->pages[n];
	if (page >= pool->last) {
		stu_log_error(0, "Failed to alloc ram page: no memory.");
		return NULL;
	}
	pool->data.last += STU_RAM_PAGE_SIZE;

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
}

