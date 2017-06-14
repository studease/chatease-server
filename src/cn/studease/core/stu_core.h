/*
 * stu_core.h
 *
 *  Created on: 2016-9-8
 *      Author: Tony Lau
 */

#ifndef STU_CORE_H_
#define STU_CORE_H_

typedef struct stu_cycle_s    stu_cycle_t;
typedef struct stu_pool_s     stu_pool_t;
typedef struct stu_chain_s    stu_chain_t;
typedef struct stu_upstream_s stu_upstream_t;

typedef enum {
	PREVIEW =    0x00,
	ENTERPRISE = 0x01
} stu_edition_t;

#define STU_OK          0
#define STU_ERROR      -1
#define STU_AGAIN      -2
#define STU_BUSY       -3
#define STU_DONE       -4
#define STU_DECLINED   -5
#define STU_ABORT      -6

#define stu_abs(value)       (((value) >= 0) ? (value) : - (value))
#define stu_max(val1, val2)  ((val1 < val2) ? (val2) : (val1))
#define stu_min(val1, val2)  ((val1 > val2) ? (val2) : (val1))

#define CR     (u_char) '\r'
#define LF     (u_char) '\n'
#define HT     (u_char) '\t'
#define CRLF   "\r\n"
#define LFHT   "\n\t"

#include "stu_string.h"
#include "stu_buf.h"
#include "stu_queue.h"
#include "stu_errno.h"
#include "stu_file.h"
#include "stu_log.h"
#include "stu_alloc.h"
#include "stu_atomic.h"
#include "stu_spinlock.h"
#include "stu_palloc.h"
#include "stu_ram.h"
#include "stu_slab.h"
#include "stu_list.h"
#include "stu_base64.h"
#include "stu_sha1.h"
#include "stu_md5.h"
#include "stu_json.h"
#include "stu_rbtree.h"
#include "stu_time.h"
#include "stu_socket.h"
#include "stu_event.h"
#include "stu_hash.h"
#include "stu_inet.h"
#include "stu_channel.h"
#include "stu_user.h"
#include "stu_connection.h"
#include "stu_shmem.h"
#include "stu_thread.h"
#include "stu_cycle.h"
#include "stu_conf_file.h"
#include "stu_protocol.h"
#include "stu_http.h"
#include "stu_upstream.h"
#include "stu_upstream_ident.h"
#include "stu_process.h"
#include "stu_filedes.h"
#include "stu_utils.h"

#endif /* STU_CORE_H_ */
