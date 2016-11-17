/*
 * stu_alloc.h
 *
 *  Created on: 2016-9-9
 *      Author: root
 */

#ifndef STU_ALLOC_H_
#define STU_ALLOC_H_

#include <stdio.h>
#include <stdlib.h>
#include "stu_config.h"
#include "stu_core.h"

void *stu_alloc(size_t size);
void *stu_calloc(size_t size);

#define stu_free  free

#endif /* STU_ALLOC_H_ */
