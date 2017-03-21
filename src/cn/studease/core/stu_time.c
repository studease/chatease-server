/*
 * stu_time.c
 *
 *  Created on: 2017-3-21
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

void
stu_timezone_update(void) {
	time_t      s;
	struct tm  *t;
	char        buf[4];

	s = time(0);
	t = localtime(&s);

	strftime(buf, 4, "%H", t);
}


void
stu_localtime(time_t s, stu_tm_t *tm) {
#if (STU_HAVE_LOCALTIME_R)
	(void) localtime_r(&s, tm);
#else
	stu_tm_t  *t;

	t = localtime(&s);
	*tm = *t;
#endif

	tm->stu_tm_mon++;
	tm->stu_tm_year += 1900;
}


void
stu_libc_localtime(time_t s, struct tm *tm) {
#if (STU_HAVE_LOCALTIME_R)
	(void) localtime_r(&s, tm);
#else
	struct tm  *t;

	t = localtime(&s);
	*tm = *t;
#endif
}


void
stu_libc_gmtime(time_t s, struct tm *tm) {
#if (STU_HAVE_LOCALTIME_R)
	(void) gmtime_r(&s, tm);
#else
	struct tm  *t;

	t = gmtime(&s);
	*tm = *t;
#endif
}

