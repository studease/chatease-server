/*
 * stu_sha1.h
 *
 *  Created on: 2016-11-28
 *      Author: Tony Lau
 */

#ifndef STU_SHA1_H_
#define STU_SHA1_H_

#include "stu_config.h"
#include "stu_core.h"

#if (STU_HAVE_OPENSSL_SHA1_H)
#include <openssl/sha.h>
#else
#include <sha.h>
#endif

typedef SHA_CTX  stu_sha1_t;

#define stu_sha1_init    SHA1_Init
#define stu_sha1_update  SHA1_Update
#define stu_sha1_final   SHA1_Final

#endif /* STU_SHA1_H_ */
