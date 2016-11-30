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

#if (STU_HAVE_OPENSSL_EVP_H)
#include <openssl/evp.h>
#else
#include <evp.h>
#endif

typedef EVP_ENCODE_CTX  stu_base64_t;

#define stu_b64_encode_new     EVP_ENCODE_CTX_new
#define stu_b64_encode_free    EVP_ENCODE_CTX_free

#define stu_b64_encode_init    EVP_EncodeInit
#define stu_b64_encode_update  EVP_EncodeUpdate
#define stu_b64_encode_final   EVP_EncodeFinal
//#define stu_b64_encode         EVP_EncodeBlock

#define stu_b64_decode_init    EVP_DecodeInit
#define stu_b64_decode_update  EVP_DecodeUpdate
#define stu_b64_decode_final   EVP_DecodeFinal
//#define stu_b64_decode         EVP_DecodeBlock

stu_int_t stu_base64_encode(u_char *t, const u_char *f, stu_int_t n);
stu_int_t stu_base64_decode(u_char *t, const u_char *f, stu_int_t n);

#endif /* STU_BASE64_H_ */
