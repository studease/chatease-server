/*
 * stu_utils.c
 *
 *  Created on: 2016-11-16
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_edition_t
stu_utils_get_edition(u_char *name, size_t len) {
	stu_edition_t  edt;

	if (stu_strncmp("preview", name, len) == 0) {
		edt = PREVIEW;
	} else if (stu_strncmp("enterprise", name, len) == 0) {
		edt = ENTERPRISE;
	}

	return edt;
}

