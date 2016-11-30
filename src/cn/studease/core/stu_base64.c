/*
 * stu_base64.c
 *
 *  Created on: 2016-11-30
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

stu_int_t
stu_base64_encode(u_char *t, const u_char *f, stu_int_t n) {
	stu_base64_t *b64;
	int           len, x;

	b64 = stu_b64_encode_new();

	stu_b64_encode_init(b64);
	x = stu_b64_encode_update(b64, t, &len, f, n);
	stu_log_debug(0, "updated x: %d", x);
	stu_b64_encode_final(b64, t, &len);

	stu_b64_encode_free(b64);

	stu_log_debug(0, "BASE64 encode: %s => %s", f, t);

	return len;
}

stu_int_t
stu_base64_decode(u_char *t, const u_char *f, stu_int_t n) {
	stu_base64_t *b64;
	int           len, x;

	b64 = stu_b64_encode_new();

	stu_b64_decode_init(b64);
	x = stu_b64_decode_update(b64, t, &len, f, n);
	stu_log_debug(0, "updated x: %d", x);
	x = stu_b64_decode_final(b64, t, &len);
	stu_log_debug(0, "updated x: %d", x);

	stu_b64_encode_free(b64);

	stu_log_debug(0, "BASE64 decode: %s => %s", f, t);

	return len;
}

