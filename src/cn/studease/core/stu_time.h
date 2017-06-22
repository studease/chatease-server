/*
 * stu_time.h
 *
 *  Created on: 2017-3-21
 *      Author: Tony Lau
 */

#ifndef STU_TIME_H_
#define STU_TIME_H_

#include "stu_config.h"
#include "stu_core.h"

typedef struct {
	time_t      sec;
	stu_uint_t  msec;
	stu_int_t   gmtoff;
} stu_time_t;

typedef stu_rbtree_key_t      stu_msec_t;
typedef stu_rbtree_key_int_t  stu_msec_int_t;

typedef struct tm             stu_tm_t;

#define stu_tm_sec            tm_sec
#define stu_tm_min            tm_min
#define stu_tm_hour           tm_hour
#define stu_tm_mday           tm_mday
#define stu_tm_mon            tm_mon
#define stu_tm_year           tm_year
#define stu_tm_wday           tm_wday
#define stu_tm_isdst          tm_isdst

#define stu_tm_sec_t          int
#define stu_tm_min_t          int
#define stu_tm_hour_t         int
#define stu_tm_mday_t         int
#define stu_tm_mon_t          int
#define stu_tm_year_t         int
#define stu_tm_wday_t         int

#define stu_tm_gmtoff         tm_gmtoff
#define stu_tm_zone           tm_zone

#define stu_timezone(isdst) (- (isdst ? timezone + 3600 : timezone) / 60)

#define stu_gettimeofday(tp)  (void) gettimeofday(tp, NULL);
#define stu_msleep(ms)        (void) usleep(ms * 1000)
#define stu_sleep(s)          (void) sleep(s)


void stu_time_init(void);
void stu_time_update(void);
u_char *stu_http_time(u_char *buf, time_t t);
u_char *stu_http_cookie_time(u_char *buf, time_t t);

void stu_timezone_update(void);
void stu_localtime(time_t s, stu_tm_t *tm);
void stu_gmtime(time_t t, stu_tm_t *tp);
void stu_libc_localtime(time_t s, struct tm *tm);
void stu_libc_gmtime(time_t s, struct tm *tm);

time_t  stu_next_time(time_t when);
#define stu_next_time_n      "mktime()"


extern volatile stu_time_t  *stu_cached_time;

#define stu_time()           stu_cached_time->sec
#define stu_timeofday()      (stu_time_t *) stu_cached_time

extern volatile stu_str_t    stu_cached_log_time;
extern volatile stu_str_t    stu_cached_http_time;
extern volatile stu_str_t    stu_cached_http_log_time;

/*
 * milliseconds elapsed since epoch and truncated to stu_msec_t,
 * used in event timers
 */
extern volatile stu_msec_t  stu_current_msec;

#endif /* STU_TIME_H_ */
