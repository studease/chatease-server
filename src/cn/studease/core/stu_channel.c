/*
 * stu_channel.c
 *
 *  Created on: 2016-12-5
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_int_t
stu_channel_init(stu_channel_t *ch, stu_str_t *id) {
	ch->id.data = (u_char *) ch  + sizeof(stu_channel_t);
	memcpy(ch->id.data, id->data, id->len);
	ch->id.len = id->len;

	return STU_OK;
}

