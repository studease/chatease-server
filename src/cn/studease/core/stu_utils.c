/*
 * stu_utils.c
 *
 *  Created on: 2016-11-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_runmode_t
stu_utils_get_runmode(stu_int_t mode) {
	stu_runmode_t  m;

	switch (mode) {
	case 0x1:
		m = ORIGIN;
		break;
	case 0X2:
		m = EDGE;
		break;
	default:
		m = STANDALONE;
		break;
	}

	return m;
}

