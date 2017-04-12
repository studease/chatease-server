/*
 * stu_slab.c
 *
 *  Created on: 2016-9-13
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static stu_slab_page_t *stu_slab_alloc_page(stu_slab_pool_t *pool);
static void stu_slab_free_page(stu_slab_pool_t *pool, stu_slab_page_t *page);


stu_slab_pool_t *
stu_slab_pool_create(size_t size) {
	stu_slab_pool_t *p;
	u_char          *c;
	stu_uint_t       n, i, page_n;
	stu_shm_t        shm;

	shm.size = size;
	if (stu_shm_alloc(&shm) != STU_OK) {
		return NULL;
	}

	p = (stu_slab_pool_t *) shm.addr;

	stu_spinlock_init(&p->lock);
	p->data.end = (u_char *) p + size;

	c = (u_char *) p + sizeof(stu_slab_pool_t);

	p->slots = (stu_slab_page_t *) c;
	n = STU_SLAB_PAGE_SHIFT - STU_SLAB_MIN_SHIFT;
	for (i = 0; i < n; i++) {
		p->slots[i].shift = STU_SLAB_MIN_SHIFT + i;
		p->slots[i].prev = p->slots[i].next = &p->slots[i];
	}

	c += n * sizeof(stu_slab_page_t);

	page_n = (stu_uint_t) ((p->data.end - c) / (sizeof(stu_slab_page_t) + STU_SLAB_PAGE_SIZE));
	stu_memzero(c, page_n * sizeof(stu_slab_page_t));
	p->pages = (stu_slab_page_t *) c;

	c += page_n * sizeof(stu_slab_page_t);

	p->last = (stu_slab_page_t *) c;
	p->free.prev = p->free.next = &p->free;

	p->data.start = p->data.last = (u_char *) stu_align_ptr((uintptr_t) c, STU_SLAB_PAGE_SIZE);

	return p;
}

void *
stu_slab_alloc(stu_slab_pool_t *pool, size_t size) {
	void *p;

	stu_spin_lock(&pool->lock);
	p = stu_slab_alloc_locked(pool, size);
	stu_spin_unlock(&pool->lock);

	return p;
}

void *
stu_slab_alloc_locked(stu_slab_pool_t *pool, size_t size) {
	stu_uint_t       shift, slot, s, m, n, i, j;
	stu_slab_page_t *page, *sentinel;
	uintptr_t        p;
	uint64_t        *bitmap;
	u_char          *c;

	if (size > STU_SLAB_MAX_SIZE) {
		if (size > STU_SLAB_PAGE_SIZE) {
			stu_log_debug(0, "slab alloc failed: size=%d, max=%d.", size, STU_SLAB_MAX_SIZE);
			return NULL;
		}

		page = stu_slab_alloc_page(pool);
		if (page == NULL) {
			return NULL;
		}

		page->shift = STU_SLAB_PAGE_SHIFT;
		page->bitmap = 1;
		page->prev = page->next = NULL;

		p = (page - pool->pages) << STU_SLAB_PAGE_SHIFT;
		p += (uintptr_t) pool->data.start;

		goto done;
	}

	if (size > STU_SLAB_MIN_SIZE) {
		shift = 1;
		for (s = size - 1; s >>= 1; shift++) { /* void */ }
		slot = shift - STU_SLAB_MIN_SHIFT;
	} else {
		size = STU_SLAB_MIN_SIZE;
		shift = STU_SLAB_MIN_SHIFT;
		slot = 0;
	}

	stu_log_debug(0, "slab alloc: %d, slot: %d", size, slot);

	sentinel = &pool->slots[slot];
	page = sentinel->next;
	if (page == sentinel) {
		page = stu_slab_alloc_page(pool);
		if (page == NULL) {
			return NULL;
		}

		page->shift = shift;

		if (shift > STU_SLAB_MID_SHIFT
				|| (shift == STU_SLAB_MID_SHIFT && __WORDSIZE == 64)) {
			page->bitmap = 0;
		} else {
			p = (page - pool->pages) << STU_SLAB_PAGE_SHIFT;
			page->bitmap = (uintptr_t) (pool->data.start + p);

			s = 1 << (STU_SLAB_MID_SHIFT - shift);
			n = s * 8;
			stu_memzero((void *) page->bitmap, n);

			n = n / (1 << shift) + (n % (1 << shift) ? 1 : 0);
			bitmap = (uint64_t *) page->bitmap;
			for (m = 1, i = 0; i < n; m <<= 1, i++) {
				*bitmap |= m;
			}
		}

		page->prev = page->next = sentinel;
		sentinel->prev = sentinel->next = page;
	}

	if (shift < STU_SLAB_MID_SHIFT
			|| (shift == STU_SLAB_MID_SHIFT && __WORDSIZE == 32)) {
		n = 1 << (STU_SLAB_MID_SHIFT - shift);
		do {
			bitmap = (uint64_t *) page->bitmap;
			for (i = 0; i < n; i++, bitmap++) {
				if (*bitmap == STU_SLAB_BUSY64) {
					continue;
				}

				c = (u_char *) bitmap;
				for (s = 0; s < 8; s++, c++) {
					if (*c == STU_SLAB_BUSY8) {
						continue;
					}

					for (m = 1, j = 0; *c & m; m <<= 1, j++) {
						/* void */
					}

					*c |= m;
					p = (i * 64 + s * 8 + j) << shift;
					p += (uintptr_t) page->bitmap;

					if (*bitmap == STU_SLAB_BUSY64) {
						bitmap++;

						for (i = i + 1; i < n; i++, bitmap++) {
							if (*bitmap != STU_SLAB_BUSY64) {
								goto done;
							}
						}

						goto full;
					}

					goto done;
				}
			}

			page = page->next;
		} while (page != sentinel);
	} else if (shift < STU_SLAB_BIG_SHIFT) {
		n = 1 << (STU_SLAB_BIG_SHIFT - shift);
		do {
			c = (u_char *) &page->bitmap;
			for (s = 0; s < n; s++, c++) {
				if (*c == STU_SLAB_BUSY8) {
					continue;
				}

				for (m = 1, j = 0; *c & m; m <<= 1, j++) {
					/* void */
				}

				*c |= m;
				p = (uintptr_t) pool->data.start;
				p += (page - pool->pages) << STU_SLAB_PAGE_SHIFT;
				p += (s * 8 + j) << shift;

				if (*c == STU_SLAB_BUSY8 && page->bitmap == STU_SLAB_BUSY64 >> (8 - n) * 8) {
					goto full;
				}

				goto done;
			}

			page = page->next;
		} while (page != sentinel);
	} else {
		n = 1 << (STU_SLAB_PAGE_SHIFT - shift);
		do {
			c = (u_char *) &page->bitmap;
			for (m = 1, j = 0; j < n; m <<= 1, j++) {
				if (*c & m) {
					continue;
				}

				*c |= m;
				p = (uintptr_t) pool->data.start;
				p += (page - pool->pages) << STU_SLAB_PAGE_SHIFT;
				p += j << shift;

				if (*c == STU_SLAB_BUSY8 >> (8 - n)) {
					goto full;
				}

				goto done;
			}

			page = page->next;
		} while (page != sentinel);
	}

full:

	page->prev->next = page->next;
	page->next->prev = page->prev;
	page->prev = page->next = NULL;

done:

	stu_log_debug(0, "slab alloc: %p", p);

	return (void *) p;
}

void *
stu_slab_calloc(stu_slab_pool_t *pool, size_t size) {
	void *p;

	stu_spin_lock(&pool->lock);
	p = stu_slab_calloc_locked(pool, size);
	stu_spin_unlock(&pool->lock);

	return p;
}

void *
stu_slab_calloc_locked(stu_slab_pool_t *pool, size_t size) {
	void *p;

	p = stu_slab_alloc_locked(pool, size);
	if (p) {
		stu_memzero(p, size);
	}

	return p;
}

void
stu_slab_free(stu_slab_pool_t *pool, void *p) {
	stu_spin_lock(&pool->lock);
	stu_slab_free_locked(pool, p);
	stu_spin_unlock(&pool->lock);
}

void
stu_slab_free_locked(stu_slab_pool_t *pool, void *p) {
	stu_slab_page_t *page, *sentinel;
	stu_uint_t       x, shift, slot, s, m, n, j;
	uint64_t        *bitmap;
	u_char          *c;

	if ((u_char *) p < pool->data.start || (u_char *) p > pool->data.end) {
		stu_log_error(0, "Failed to free slab: outside of pool, p=%p.", p);
		return;
	}

	x = ((u_char *) p - pool->data.start) >> STU_SLAB_PAGE_SHIFT;
	page = &pool->pages[x];
	shift = page->shift;
	slot = shift - STU_SLAB_MIN_SHIFT;
	sentinel = &pool->slots[slot];

	j = ((u_char *) p - pool->data.start - (x << STU_SLAB_PAGE_SHIFT)) >> shift;
	if (shift < STU_SLAB_MID_SHIFT
			|| (shift == STU_SLAB_MID_SHIFT && __WORDSIZE == 32)) {
		s = j >> 3;
		//i = s >> 3;

		bitmap = (uint64_t *) page->bitmap;
		c = (u_char *) bitmap + s;

		m = 1 << (j % 8);
		*c &= ~m;

		if (page->prev == NULL) {
			goto append;
		}

		n = 1 << (STU_SLAB_MID_SHIFT - shift);
		while (n--) {
			if (*bitmap) {
				goto done;
			}
			bitmap++;
		}
		stu_slab_free_page(pool, page);

		goto done;
	} else {
		m = 1 << j;
		page->bitmap &= ~m;

		if (page->bitmap == 0) {
			stu_slab_free_page(pool, page);
			goto done;
		}

		if (page->prev == NULL) {
			goto append;
		}

		goto done;
	}

append:

	page->prev = sentinel->prev;
	page->next = sentinel;
	sentinel->prev->next = page;
	sentinel->prev = page;

done:

	stu_log_debug(0, "slab free: %p", p);
}


static stu_slab_page_t *
stu_slab_alloc_page(stu_slab_pool_t *pool) {
	stu_slab_page_t *page;
	stu_uint_t       n;

	page = pool->free.next;
	if (page != &pool->free) {
		pool->free.next = page->next;
		page->next->prev = &pool->free;
		return page;
	}

	n = (pool->data.last - pool->data.start) >> STU_SLAB_PAGE_SHIFT;
	page = &pool->pages[n];
	if (page >= pool->last) {
		stu_log_error(0, "Failed to alloc slab page: no memory.");
		return NULL;
	}
	pool->data.last += STU_SLAB_PAGE_SIZE;

	return page;
}

static void
stu_slab_free_page(stu_slab_pool_t *pool, stu_slab_page_t *page) {
	if (page->prev != NULL && page->next != NULL) {
		page->prev->next = page->next;
		page->next->prev = page->prev;
	}
	page->prev = pool->free.prev;
	page->next = &pool->free;

	pool->free.prev->next = page;
	pool->free.prev = page;
}

