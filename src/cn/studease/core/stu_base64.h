/*
 * stu_base64.h
 *
 *  Created on: 2016-11-30
 *      Author: Tony Lau
 */

#ifndef STU_BASE64_H_
#define STU_BASE64_H_

#include "stu_config.h"
#include "stu_core.h"

#define stu_base64_encoded_length(len)  (((len + 2) / 3) * 4)
#define stu_base64_decoded_length(len)  (((len + 3) / 4) * 3)

void stu_base64_encode(stu_str_t *dst, stu_str_t *src);
stu_int_t stu_base64_decode(stu_str_t *dst, stu_str_t *src);

#endif /* STU_BASE64_H_ */
