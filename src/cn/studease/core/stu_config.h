/*
 * stu_config.h
 *
 *  Created on: 2016-9-8
 *      Author: Tony Lau
 */

#ifndef STU_CONFIG_H_
#define STU_CONFIG_H_

#include <errno.h>
#include <stddef.h>
#include <math.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define __NAME    "chatease-server"
#define __VERSION "1.0.14"
#define __LOGGER  4 // debug: 0 - 7, log: 8, error: 9

#define STU_LINUX 1
#define STU_WIN32 !STU_LINUX

#define STU_HAVE_LOCALTIME_R     1
#define STU_HAVE_OPENSSL_EVP_H   1
#define STU_HAVE_OPENSSL_SHA1_H  1
#define STU_HAVE_OPENSSL_MD5_H   1

#define stu_signal_helper(n)     SIG##n
#define stu_signal_value(n)      stu_signal_helper(n)

#define STU_SHUTDOWN_SIGNAL      QUIT
#define STU_CHANGEBIN_SIGNAL     XCPU

typedef signed long         stu_int_t;
typedef unsigned long       stu_uint_t;
typedef unsigned char       stu_bool_t;
typedef short               stu_short_t;
typedef double              stu_double_t;

#define TRUE                1
#define FALSE               0

#define STU_ALIGNMENT       sizeof(unsigned long)

#define stu_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define stu_align_ptr(p, a) \
	(u_char *) (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

#define stu_inline          inline

#endif /* STU_CONFIG_H_ */
