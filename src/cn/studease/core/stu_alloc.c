/*
 * stu_alloc.c
 *
 *  Created on: 2016-9-9
 *      Author: Tony Lau
 */

#include <stdio.h>
#include <stdlib.h>
#include "stu_config.h"
#include "stu_core.h"

void *
stu_alloc(size_t size) {
	void  *p;

	p = malloc(size);
	if (p == NULL) {
		stu_log_error(stu_errno, "malloc(%uz) failed", size);
	}

	stu_log_debug(0, "malloc: %p, %zd", p, size);

	return p;
}

void *
stu_calloc(size_t size) {
	void  *p;

	p = stu_alloc(size);
	if (p) {
		stu_memzero(p, size);
	}

	return p;
}

