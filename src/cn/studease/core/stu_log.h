/*
 * stu_log.h
 *
 *  Created on: 2016-9-8
 *      Author: Tony Lau
 */

#ifndef STU_LOG_H_
#define STU_LOG_H_

#include <stdarg.h>
#include "stu_config.h"
#include "stu_core.h"

#define STU_LOG_RECORD_MAX_LEN  1024

static const stu_str_t STU_LOG_PREFIX = stu_string("[L O G]");
static const stu_str_t STU_DEBUG_PREFIX = stu_string("[DEBUG]");
static const stu_str_t STU_ERROR_PREFIX = stu_string("[ERROR]");

#define STU_LOG_FILE_LINE                  "[" __FILE__ ":%d] " LFHT
#define stu_log(fmt, args...)              stu_log_c(STU_LOG_FILE_LINE fmt "\n", __LINE__, ##args)
#define stu_log_debug(level, fmt, args...) stu_log_c_debug(level, STU_LOG_FILE_LINE fmt "\n", __LINE__, ##args)
#define stu_log_error(errno, fmt, args...) stu_log_c_error(errno, STU_LOG_FILE_LINE fmt, __LINE__, ##args)

stu_int_t  stu_log_init(stu_file_t *file);

void stu_log_c(const char *fmt, ...);
void stu_log_c_debug(stu_int_t level, const char *fmt, ...);
void stu_log_c_error(stu_int_t err, const char *fmt, ...);

#endif /* STU_LOG_H_ */
