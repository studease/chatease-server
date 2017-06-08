/*
 * stu_time.c
 *
 *  Created on: 2017-3-21
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

char *week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


void
stu_timezone_update(void) {
	time_t     s;
	struct tm *t;
	char       buf[4];

	s = time(NULL);
	t = localtime(&s);

	strftime(buf, 4, "%H", t);
}


void
stu_localtime(time_t s, stu_tm_t *tm) {
#if (STU_HAVE_LOCALTIME_R)
	(void) localtime_r(&s, tm);
#else
	stu_tm_t *t;

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
	struct tm *t;

	t = localtime(&s);
	*tm = *t;
#endif
}


void
stu_gmtime(time_t t, stu_tm_t *tp) {
	stu_int_t   yday;
	stu_uint_t  n, sec, min, hour, mday, mon, year, wday, days, leap;

	/* the calculation is valid for positive time_t only */
	n = (stu_uint_t) t;
	days = n / 86400;

	/* January 1, 1970 was Thursday */
	wday = (4 + days) % 7;

	n %= 86400;
	hour = n / 3600;
	n %= 3600;
	min = n / 60;
	sec = n % 60;

	/*
	 * the algorithm based on Gauss' formula,
	 * see src/http/stu_http_parse_time.c
	 */

	/* days since March 1, 1 BC */
	days = days - (31 + 28) + 719527;

	/*
	 * The "days" should be adjusted to 1 only, however, some March 1st's go
	 * to previous year, so we adjust them to 2.  This causes also shift of the
	 * last February days to next year, but we catch the case when "yday"
	 * becomes negative.
	 */
	year = (days + 2) * 400 / (365 * 400 + 100 - 4 + 1);
	yday = days - (365 * year + year / 4 - year / 100 + year / 400);

	if (yday < 0) {
		leap = (year % 4 == 0) && (year % 100 || (year % 400 == 0));
		yday = 365 + leap + yday;
		year--;
	}

	/*
	 * The empirical formula that maps "yday" to month.
	 * There are at least 10 variants, some of them are:
	 *     mon = (yday + 31) * 15 / 459
	 *     mon = (yday + 31) * 17 / 520
	 *     mon = (yday + 31) * 20 / 612
	 */
	mon = (yday + 31) * 10 / 306;

	/* the Gauss' formula that evaluates days before the month */
	mday = yday - (367 * mon / 12 - 30) + 1;

	if (yday >= 306) {
		year++;
		mon -= 10;

		/*
		 * there is no "yday" in Win32 SYSTEMTIME
		 *
		 * yday -= 306;
		 */
	} else {
		mon += 2;

		/*
		 * there is no "yday" in Win32 SYSTEMTIME
		 *
		 * yday += 31 + 28 + leap;
		 */
	}

	tp->stu_tm_sec = (stu_tm_sec_t) sec;
	tp->stu_tm_min = (stu_tm_min_t) min;
	tp->stu_tm_hour = (stu_tm_hour_t) hour;
	tp->stu_tm_mday = (stu_tm_mday_t) mday;
	tp->stu_tm_mon = (stu_tm_mon_t) mon;
	tp->stu_tm_year = (stu_tm_year_t) year;
	tp->stu_tm_wday = (stu_tm_wday_t) wday;
}

void
stu_libc_gmtime(time_t s, struct tm *tm) {
#if (STU_HAVE_LOCALTIME_R)
	(void) gmtime_r(&s, tm);
#else
	struct tm *t;

	t = gmtime(&s);
	*tm = *t;
#endif
}

