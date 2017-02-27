/*
 * stu_json.h
 *
 *  Created on: 2017-2-27
 *      Author: Tony Lau
 */

#ifndef STU_JSON_H_
#define STU_JSON_H_

#include "stu_config.h"
#include "stu_core.h"

#define STU_JSON_TYPE_STRING 0x01
#define STU_JSON_TYPE_NUMBER 0x02
#define STU_JSON_TYPE_INT    0x04
#define STU_JSON_TYPE_DOUBLE 0x0A
#define STU_JSON_TYPE_ARRAY  0x10
#define STU_JSON_TYPE_OBJECT 0x20

typedef struct {
	u_char  type;
	u_char *key;
	void   *value;
} stu_json;

#endif /* STU_JSON_H_ */
