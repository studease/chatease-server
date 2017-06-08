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

void stu_timezone_update(void);

void stu_localtime(time_t s, stu_tm_t *tm);
void stu_libc_localtime(time_t s, struct tm *tm);

void stu_gmtime(time_t t, stu_tm_t *tp);
void stu_libc_gmtime(time_t s, struct tm *tm);


#define stu_gettimeofday(tp)  (void) gettimeofday(tp, NULL);
#define stu_msleep(ms)        (void) usleep(ms * 1000)
#define stu_sleep(s)          (void) sleep(s)

#endif /* STU_TIME_H_ */
