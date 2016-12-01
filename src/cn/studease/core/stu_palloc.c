/*
 * stu_palloc.c
 *
 *  Created on: 2016-9-12
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

static void *stu_palloc_next(stu_pool_t *pool, size_t size);
static void *stu_palloc_large(stu_pool_t *pool, size_t size);


stu_pool_t *
stu_pool_create(size_t size) {
	stu_pool_t *p;

	p = stu_alloc(size);
	if (p == NULL) {
		stu_log_error(0, "Failed to create pool, size=%zu.", size);
		return NULL;
	}

	stu_spinlock_init(&p->lock);

	p->data.start = p->data.last = (u_char *) p + sizeof(stu_pool_t);
	p->data.end = (u_char *) p + size;

	p->prev = p->next = NULL;
	p->last = p;

	size -= sizeof(stu_pool_t);
	p->max = (size < STU_POOL_MAX_ALLOC_SIZE) ? size : STU_POOL_MAX_ALLOC_SIZE;
	p->large = NULL;
	p->failed = 0;

	return p;
}

void
stu_pool_destroy(stu_pool_t *pool) {
	stu_pool_t      *p;
	stu_base_pool_t *l;

	if (pool->prev) {
		pool->prev->last = pool->prev;
		pool->prev->next = NULL;
	}

	for (p = pool; p; p = p->next) {
		for (l = p->large; l; l = l->next) {
			stu_log_debug(0, "free large: %p", l);
			stu_free(l);
		}

		stu_log_debug(0, "free pool: %p", p);
		stu_free(p);
	}
}

void
stu_pool_reset(stu_pool_t *pool) {
	stu_pool_t      *p;
	stu_base_pool_t *l;

	for (p = pool; p; p = p->next) {
		for (l = p->large; l; l = l->next) {
			stu_log_debug(0, "free large: %p", l);
			stu_free(l);
		}

		p->data.last = (u_char *) p + sizeof(stu_pool_t);
		p->last = p;
		p->large = NULL;
		p->failed = 0;
	}
}

void
stu_base_pool_reset(stu_base_pool_t *pool) {
	stu_base_pool_t *p;

	for (p = pool; p; p = p->next) {
		p->data.last = p->data.start;
	}
}

void *
stu_palloc(stu_pool_t *pool, size_t size) {
	u_char     *m;
	stu_pool_t *p;

	if (size <= pool->max) {
		p = pool->last;
		do {
			m = p->data.last;
			if ((size_t) (p->data.end - m) >= size) {
				p->data.last += size;
				return m;
			}

			p = p->next;
		} while (p);

		return stu_palloc_next(pool, size);
	}

	return stu_palloc_large(pool, size);
}

void *
stu_pcalloc(stu_pool_t *pool, size_t size) {
	void *p;

	p = stu_palloc(pool, size);
	if (p) {
		stu_memzero(p, size);
	}

	return p;
}


void *
stu_base_palloc(stu_base_pool_t *pool, size_t size) {
	u_char     *m;

	m = pool->data.last;
	if ((size_t) (pool->data.end - m) >= size) {
		pool->data.last += size;
		return m;
	}

	return NULL;
}

void *
stu_base_pcalloc(stu_base_pool_t *pool, size_t size) {
	void *p;

	p = stu_base_palloc(pool, size);
	if (p) {
		stu_memzero(p, size);
	}

	return p;
}


static void *
stu_palloc_next(stu_pool_t *pool, size_t size) {
	u_char     *m;
	size_t      s;
	stu_pool_t *x, *p;

	s = (size_t) (pool->data.end - (u_char *) pool);

	x = stu_alloc(s);
	if (x == NULL) {
		stu_log_error(0, "Failed to alloc block, size=%zu", s);
		return NULL;
	}

	stu_spinlock_init(&x->lock);

	x->data.start = m = (u_char *) x + sizeof(stu_pool_t);
	x->data.last = m + size;
	x->data.end = (u_char *) x + s;

	x->max = pool->max;
	x->large = NULL;
	x->failed = 0;

	for (p = pool->last; p->next; p = p->next) {
		if (p->failed++ > 4) {
			pool->last = p->last = p->next;
		}
	}

	p->next = x;
	x->prev = p;
	x->next = NULL;
	x->last = x;

	return m;
}

static void *
stu_palloc_large(stu_pool_t *pool, size_t size) {
	void            *m;
	size_t           s;
	stu_base_pool_t *l;

	s = (size_t) (sizeof(stu_base_pool_t) + size);

	l = stu_alloc(s);
	if (l == NULL) {
		stu_log_error(0, "Failed to alloc large, size=%zu", s);
		return NULL;
	}

	stu_spinlock_init(&l->lock);

	l->data.start = m = (u_char *) l + sizeof(stu_base_pool_t);
	l->data.last = l->data.end = (u_char *) l + s;

	l->prev = NULL;
	l->next = pool->large;
	if (pool->large) {
		pool->large->prev = l;
	}
	pool->large = l;

	return m;
}

