/*
 * stu_time.c
 *
 *  Created on: 2017-3-21
 *      Author: Tony Lau
 */

#include "stu_config.h"
#include "stu_core.h"

/*
 * The time may be updated by signal handler or by several threads.
 * The time update operations are rare and require to hold the stu_time_lock.
 * The time read operations are frequent, so they are lock-free and get time
 * values and strings from the current slot.  Thus thread may get the corrupted
 * values only if it is preempted while copying and then it is not scheduled
 * to run more than STU_TIME_SLOTS seconds.
 */

#define STU_TIME_SLOTS   64

static stu_uint_t        slot;
static stu_mutex_t       stu_time_lock;

volatile stu_msec_t      stu_current_msec;
volatile stu_time_t     *stu_cached_time;
volatile stu_str_t       stu_cached_log_time;
volatile stu_str_t       stu_cached_http_time;
volatile stu_str_t       stu_cached_http_log_time;

#if !(STU_WIN32)
/*
 * localtime() and localtime_r() are not Async-Signal-Safe functions, therefore,
 * they must not be called by a signal handler, so we use the cached
 * GMT offset value. Fortunately the value is changed only two times a year.
 */
static stu_int_t         cached_gmtoff;
#endif

static stu_time_t        cached_time[STU_TIME_SLOTS];
static u_char            cached_log_time[STU_TIME_SLOTS]
                                 [sizeof("1970/09/28 12:00:00")];
static u_char            cached_http_time[STU_TIME_SLOTS]
                                 [sizeof("Mon, 28 Sep 1970 06:00:00 GMT")];
static u_char            cached_http_log_time[STU_TIME_SLOTS]
                                 [sizeof("28/Sep/1970:12:00:00 +0600")];

static char *week[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


void
stu_time_init(void) {
	stu_cached_log_time.len = sizeof("1970/09/28 12:00:00") - 1;
	stu_cached_http_time.len = sizeof("Mon, 28 Sep 1970 06:00:00 GMT") - 1;
	stu_cached_http_log_time.len = sizeof("28/Sep/1970:12:00:00 +0600") - 1;

	stu_cached_time = &cached_time[0];

	stu_mutex_init(&stu_time_lock, NULL);
	stu_time_update();
}

void
stu_time_update(void) {
	u_char          *p0, *p1, *p2;
	stu_tm_t         tm, gmt;
	time_t           sec;
	stu_uint_t       msec;
	stu_time_t      *tp;
	struct timeval   tv;

	if (stu_mutex_trylock(&stu_time_lock)) {
		return;
	}

	stu_gettimeofday(&tv);

	sec = tv.tv_sec;
	msec = tv.tv_usec / 1000;

	stu_current_msec = (stu_msec_t) sec * 1000 + msec;

	tp = &cached_time[slot];
	if (tp->sec == sec) {
		tp->msec = msec;
		stu_mutex_unlock(&stu_time_lock);
		return;
	}

	if (slot == STU_TIME_SLOTS - 1) {
		slot = 0;
	} else {
		slot++;
	}

	tp = &cached_time[slot];
	tp->sec = sec;
	tp->msec = msec;

	stu_gmtime(sec, &gmt);

	p0 = &cached_http_time[slot][0];
	(void) stu_sprintf(p0, "%s, %02d %s %4d %02d:%02d:%02d GMT",
			week[gmt.stu_tm_wday], gmt.stu_tm_mday, months[gmt.stu_tm_mon - 1],
			gmt.stu_tm_year, gmt.stu_tm_hour, gmt.stu_tm_min, gmt.stu_tm_sec);

#if (STU_HAVE_GETTIMEZONE)

	tp->gmtoff = stu_gettimezone();
	stu_gmtime(sec + tp->gmtoff * 60, &tm);

#elif (STU_HAVE_GMTOFF)

	stu_localtime(sec, &tm);
	cached_gmtoff = (stu_int_t) (tm.stu_tm_gmtoff / 60);
	tp->gmtoff = cached_gmtoff;

#else

	stu_localtime(sec, &tm);
	cached_gmtoff = stu_timezone(tm.stu_tm_isdst);
	tp->gmtoff = cached_gmtoff;

#endif

	p1 = &cached_log_time[slot][0];
	(void) stu_sprintf(p1, "%4d/%02d/%02d %02d:%02d:%02d",
			tm.stu_tm_year, tm.stu_tm_mon, tm.stu_tm_mday,
			tm.stu_tm_hour, tm.stu_tm_min, tm.stu_tm_sec);

	p2 = &cached_http_log_time[slot][0];
	(void) stu_sprintf(p2, "%02d/%s/%d:%02d:%02d:%02d %c%02d%02d",
			tm.stu_tm_mday, months[tm.stu_tm_mon - 1], tm.stu_tm_year,
			tm.stu_tm_hour, tm.stu_tm_min, tm.stu_tm_sec,
			tp->gmtoff < 0 ? '-' : '+', stu_abs(tp->gmtoff / 60), stu_abs(tp->gmtoff % 60));

	stu_memory_barrier();

	stu_cached_time = tp;
	stu_cached_log_time.data = p1;
	stu_cached_http_time.data = p0;
	stu_cached_http_log_time.data = p2;

	stu_mutex_unlock(&stu_time_lock);
}

u_char *
stu_http_time(u_char *buf, time_t t) {
	stu_tm_t  tm;

	stu_gmtime(t, &tm);

	return stu_sprintf(buf, "%s, %02d %s %4d %02d:%02d:%02d GMT",
			week[tm.stu_tm_wday], tm.stu_tm_mday, months[tm.stu_tm_mon - 1],
			tm.stu_tm_year, tm.stu_tm_hour, tm.stu_tm_min, tm.stu_tm_sec);
}

u_char *
stu_http_cookie_time(u_char *buf, time_t t) {
	stu_tm_t  tm;

	stu_gmtime(t, &tm);

	/*
	 * Netscape 3.x does not understand 4-digit years at all and
	 * 2-digit years more than "37"
	 */

	return stu_sprintf(buf, (tm.stu_tm_year > 2037) ? "%s, %02d-%s-%d %02d:%02d:%02d GMT" : "%s, %02d-%s-%02d %02d:%02d:%02d GMT",
			week[tm.stu_tm_wday], tm.stu_tm_mday, months[tm.stu_tm_mon - 1],
			(tm.stu_tm_year > 2037) ? tm.stu_tm_year : tm.stu_tm_year % 100,
			tm.stu_tm_hour, tm.stu_tm_min, tm.stu_tm_sec);
}


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
		 * yday -= 306;
		 */
	} else {
		mon += 2;

		/*
		 * there is no "yday" in Win32 SYSTEMTIME
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


time_t
stu_next_time(time_t when) {
	time_t     now, next;
	struct tm  tm;

	now = stu_time();
	stu_libc_localtime(now, &tm);

	tm.tm_hour = (int) (when / 3600);
	when %= 3600;
	tm.tm_min = (int) (when / 60);
	tm.tm_sec = (int) (when % 60);

	next = mktime(&tm);
	if (next == -1) {
		return -1;
	}

	if (next - now > 0) {
		return next;
	}

	tm.tm_mday++;

	/* mktime() should normalize a date (Jan 32, etc) */
	next = mktime(&tm);
	if (next != -1) {
		return next;
	}

	return -1;
}
