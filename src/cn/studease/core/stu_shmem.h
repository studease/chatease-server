/*
 * stu_shmem.h
 *
 *  Created on: 2016-9-8
 *      Author: Tony Lau
 */

#ifndef STU_SHMEM_H_
#define STU_SHMEM_H_

#include "stu_config.h"
#include "stu_core.h"
#include <stdint.h>

typedef struct {
	u_char     *addr;
	size_t      size;
} stu_shm_t;

stu_int_t stu_shm_alloc(stu_shm_t *shm);
void stu_shm_free(stu_shm_t *shm);

#endif /* STU_SHMEM_H_ */
