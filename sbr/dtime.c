
/*
 * dtime.c -- time/date routines
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>   /* for snprintf() */
#include <h/nmh.h>
#include <h/tws.h>

#if !defined(HAVE_STRUCT_TM_TM_GMTOFF) && !defined(HAVE_TZSET)
# include <sys/timeb.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if !defined(HAVE_STRUCT_TM_TM_GMTOFF) && defined(HAVE_TZSET)
extern int daylight;
extern long timezone;
extern char *tzname[];
#endif

#ifndef	abs
# define abs(a) (a >= 0 ? a : -a)
#endif

/*
 * The number of days in the year, accounting for leap years
 */
#define	dysize(y)	\
	(((y) % 4) ? 365 : (((y) % 100) ? 366 : (((y) % 400) ? 365 : 366)))

char *tw_moty[] = {
    "Jan", "Feb", "Mar", "Apr",
    "May", "Jun", "Jul", "Aug",
    "Sep", "Oct", "Nov", "Dec",
    NULL
};

char *tw_dotw[] = {
    "Sun", "Mon", "Tue",
    "Wed", "Thu", "Fri",
    "Sat", NULL
};

char *tw_ldotw[] = {
    "Sunday",    "Monday",   "Tuesday",
    "Wednesday", "Thursday", "Friday",
    "Saturday",  NULL
};

struct zone {
    char *std;
    char *dst;
    int shift;
};

static struct zone zones[] = {
    { "GMT", "BST", 0 },
    { "EST", "EDT", -5 },
    { "CST", "CDT", -6 },
    { "MST", "MDT", -7 },
    { "PST", "PDT", -8 },
#if 0
/* RFC1123 specifies do not use military TZs */
    { "A", NULL, -1 },
    { "B", NULL, -2 },
    { "C", NULL, -3 },
    { "D", NULL, -4 },
    { "E", NULL, -5 },
    { "F", NULL, -6 },
    { "G", NULL, -7 },
    { "H", NULL, -8 },
    { "I", NULL, -9 },
    { "K", NULL, -10 },
    { "L", NULL, -11 },
    { "M", NULL, -12 },
    { "N", NULL, 1 },
#ifndef	HUJI
    { "O", NULL, 2 },
#else
    { "JST", "JDT", 2 },
#endif
    { "P", NULL, 3 },
    { "Q", NULL, 4 },
    { "R", NULL, 5 },
    { "S", NULL, 6 },
    { "T", NULL, 7 },
    { "U", NULL, 8 },
    { "V", NULL, 9 },
    { "W", NULL, 10 },
    { "X", NULL, 11 },
    { "Y", NULL, 12 },
#endif
    { NULL, NULL, 0 }
};

static int dmsize[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};


/*
 * Get current time (adjusted for local time
 * zone and daylight savings time) expressed
 * as nmh "broken-down" time structure.
 */

struct tws *
dlocaltimenow (void)
{
    time_t clock;

    time (&clock);
    return dlocaltime (&clock);
}


/*
 * Take clock value and return pointer to nmh time structure
 * containing "broken-down" time.  The time is adjusted for
 * local time zone and daylight savings time.
 */

struct tws *
dlocaltime (time_t *clock)
{
    static struct tws tw;
    struct tm *tm;

#if !defined(HAVE_STRUCT_TM_TM_GMTOFF) && !defined(HAVE_TZSET)
    struct timeb tb;
#endif

    if (!clock)
	return NULL;

    tm = localtime (clock);

    tw.tw_sec  = tm->tm_sec;
    tw.tw_min  = tm->tm_min;
    tw.tw_hour = tm->tm_hour;
    tw.tw_mday = tm->tm_mday;
    tw.tw_mon  = tm->tm_mon;

    /*
     * tm_year is always "year - 1900".
     * So we correct for this.
     */
    tw.tw_year = tm->tm_year + 1900;
    tw.tw_wday = tm->tm_wday;
    tw.tw_yday = tm->tm_yday;

    tw.tw_flags = TW_NULL;
    if (tm->tm_isdst)
	tw.tw_flags |= TW_DST;

#ifdef HAVE_TM_GMTOFF
    tw.tw_zone = tm->tm_gmtoff / 60;
    if (tm->tm_isdst)			/* if DST is in effect */
	tw.tw_zone -= 60;		/* reset to normal offset */
#else
# ifdef HAVE_TZSET
    tzset();
    tw.tw_zone = -(timezone / 60);
# else
    ftime (&tb);
    tw.tw_zone = -tb.timezone;
# endif
#endif

    tw.tw_flags &= ~TW_SDAY;
    tw.tw_flags |= TW_SEXP;
    tw.tw_flags &= ~TW_SZONE;
    tw.tw_flags |= TW_SZEXP;

    tw.tw_clock = *clock;

    return (&tw);
}


/*
 * Take clock value and return pointer to nmh time
 * structure containing "broken-down" time.  Time is
 * expressed in UTC (Coordinated Universal Time).
 */

struct tws *
dgmtime (time_t *clock)
{
    static struct tws tw;
    struct tm *tm;

    if (!clock)
	return NULL;

    tm = gmtime (clock);

    tw.tw_sec  = tm->tm_sec;
    tw.tw_min  = tm->tm_min;
    tw.tw_hour = tm->tm_hour;
    tw.tw_mday = tm->tm_mday;
    tw.tw_mon  = tm->tm_mon;

    /*
     * tm_year is always "year - 1900"
     * So we correct for this.
     */
    tw.tw_year = tm->tm_year + 1900;
    tw.tw_wday = tm->tm_wday;
    tw.tw_yday = tm->tm_yday;

    tw.tw_flags = TW_NULL;
    if (tm->tm_isdst)
	tw.tw_flags |= TW_DST;

    tw.tw_zone = 0;

    tw.tw_flags &= ~TW_SDAY;
    tw.tw_flags |= TW_SEXP;
    tw.tw_flags &= ~TW_SZONE;
    tw.tw_flags |= TW_SZEXP;

    tw.tw_clock = *clock;

    return (&tw);
}


/*
 * Using a nmh "broken-down" time structure,
 * produce a 26-byte date/time string, such as
 *
 *	Tue Jan 14 17:49:03 1992\n\0
 */

char *
dctime (struct tws *tw)
{
    static char buffer[26];

    if (!tw)
	return NULL;

    snprintf (buffer, sizeof(buffer), "%.3s %.3s %02d %02d:%02d:%02d %.4d\n",
	    tw_dotw[tw->tw_wday], tw_moty[tw->tw_mon], tw->tw_mday,
	    tw->tw_hour, tw->tw_min, tw->tw_sec,
	    tw->tw_year < 100 ? tw->tw_year + 1900 : tw->tw_year);

    return buffer;
}


/*
 * Produce a date/time string of the form
 *
 *	Mon, 16 Jun 1992 15:30:48 -700 (or)
 *	Mon, 16 Jun 1992 15:30:48 EDT
 *
 * for the current time, as specified by rfc822.
 * The first form is required by rfc1123.
 */

char *
dtimenow (int alpha_timezone)
{
    time_t clock;

    time (&clock);
    return dtime (&clock, alpha_timezone);
}


/*
 * Using a local calendar time value, produce
 * a date/time string of the form
 *
 *	Mon, 16 Jun 1992 15:30:48 -700  (or)
 *	Mon, 16 Jun 1992 15:30:48 EDT
 *
 * as specified by rfc822.  The first form is required
 * by rfc1123 for outgoing messages.
 */

char *
dtime (time_t *clock, int alpha_timezone)
{
    if (alpha_timezone)
	/* use alpha-numeric timezones */
	return dasctime (dlocaltime (clock), TW_NULL);
    else
	/* use numeric timezones */
	return dasctime (dlocaltime (clock), TW_ZONE);
}


/*
 * Using a nmh "broken-down" time structure, produce
 * a date/time string of the form
 *
 *	Mon, 16 Jun 1992 15:30:48 -0700
 *
 * as specified by rfc822 and rfc1123.
 */

char *
dasctime (struct tws *tw, int flags)
{
    char buffer[80];
    static char result[80];

    if (!tw)
	return NULL;

    /* Display timezone if known */
    if ((tw->tw_flags & TW_SZONE) == TW_SZNIL)
	result[0] = '\0';
    else
	snprintf(result, sizeof(result), " %s", dtimezone(tw->tw_zone, tw->tw_flags | flags));

    snprintf(buffer, sizeof(buffer), "%02d %s %0*d %02d:%02d:%02d%s",
	    tw->tw_mday, tw_moty[tw->tw_mon],
	    tw->tw_year < 100 ? 2 : 4, tw->tw_year,
	    tw->tw_hour, tw->tw_min, tw->tw_sec, result);

    if ((tw->tw_flags & TW_SDAY) == TW_SEXP)
	snprintf (result, sizeof(result), "%s, %s", tw_dotw[tw->tw_wday], buffer);
    else
	if ((tw->tw_flags & TW_SDAY) == TW_SNIL)
	    strncpy (result, buffer, sizeof(result));
	else
	    snprintf (result, sizeof(result), "%s (%s)", buffer, tw_dotw[tw->tw_wday]);

    return result;
}


/*
 * Get the timezone for given offset
 */

char *
dtimezone (int offset, int flags)
{
    int hours, mins;
    struct zone *z;
    static char buffer[10];

    if (offset < 0) {
	mins = -((-offset) % 60);
	hours = -((-offset) / 60);
    } else {
	mins = offset % 60;
	hours = offset / 60;
    }

    if (!(flags & TW_ZONE) && mins == 0) {
#if defined(HAVE_TZSET) && defined(HAVE_TZNAME)
	tzset();
	return ((flags & TW_DST) ? tzname[1] : tzname[0]);
#else
	for (z = zones; z->std; z++)
	    if (z->shift == hours)
		return (z->dst && (flags & TW_DST) ? z->dst : z->std);
#endif
    }

#ifdef ADJUST_NUMERIC_ONLY_TZ_OFFSETS_WRT_DST
    if (flags & TW_DST)
	hours += 1;
#endif /* ADJUST_NUMERIC_ONLY_TZ_OFFSETS_WRT_DST */
    snprintf (buffer, sizeof(buffer), "%s%02d%02d",
		offset < 0 ? "-" : "+", abs (hours), abs (mins));
    return buffer;
}


/*
 * Convert nmh time structure for local "broken-down"
 * time to calendar time (clock value).  This routine
 * is based on the gtime() routine written by Steven Shafer
 * at CMU.  It was forwarded to MTR by Jay Lepreau at Utah-CS.
 */

time_t
dmktime (struct tws *tw)
{
    int i, sec, min, hour, mday, mon, year;
    time_t result;

    if (tw->tw_clock != 0)
	return tw->tw_clock;

    if ((sec = tw->tw_sec) < 0 || sec > 61
	    || (min = tw->tw_min) < 0 || min > 59
	    || (hour = tw->tw_hour) < 0 || hour > 23
	    || (mday = tw->tw_mday) < 1 || mday > 31
	    || (mon = tw->tw_mon + 1) < 1 || mon > 12)
	return (tw->tw_clock = (time_t) -1);

    year = tw->tw_year;

    result = 0;
    if (year < 1970)
      year += 1900;

    if (year < 1970)
      year += 100;

    for (i = 1970; i < year; i++)
	result += dysize (i);
    if (dysize (year) == 366 && mon >= 3)
	result++;
    while (--mon)
	result += dmsize[mon - 1];
    result += mday - 1;
    result = 24 * result + hour;
    result = 60 * result + min;
    result = 60 * result + sec;
    result -= 60 * tw->tw_zone;
    if (tw->tw_flags & TW_DST)
	result -= 60 * 60;

    return (tw->tw_clock = result);
}


/*
 * Simple calculation of day of the week.  Algorithm
 * used is Zeller's congruence.  We assume that
 * if tw->tw_year < 100, then the century = 19.
 */

void
set_dotw (struct tws *tw)
{
    int month, day, year, century;

    month = tw->tw_mon - 1;
    day = tw->tw_mday;
    year = tw->tw_year % 100;
    century = tw->tw_year < 100 ? 19 : tw->tw_year / 100;

    if (month <= 0) {
	month += 12;
	if (--year < 0) {
	    year += 100;
	    century--;
	}
    }

    tw->tw_wday =
	((26 * month - 2) / 10 + day + year + year / 4
	    - 3 * century / 4 + 1) % 7;
    if (tw->tw_wday < 0)
	tw->tw_wday += 7;

    tw->tw_flags &= ~TW_SDAY, tw->tw_flags |= TW_SIMP;
}


/*
 * Copy nmh time structure
 */

void
twscopy (struct tws *tb, struct tws *tw)
{
    *tb = *tw;  /* struct copy */

#if 0
    tb->tw_sec   = tw->tw_sec;
    tb->tw_min   = tw->tw_min;
    tb->tw_hour  = tw->tw_hour;
    tb->tw_mday  = tw->tw_mday;
    tb->tw_mon   = tw->tw_mon;
    tb->tw_year  = tw->tw_year;
    tb->tw_wday  = tw->tw_wday;
    tb->tw_yday  = tw->tw_yday;
    tb->tw_zone  = tw->tw_zone;
    tb->tw_clock = tw->tw_clock;
    tb->tw_flags = tw->tw_flags;
#endif
}


/*
 * Compare two nmh time structures
 */

int
twsort (struct tws *tw1, struct tws *tw2)
{
    time_t c1, c2;

    if (tw1->tw_clock == 0)
	dmktime (tw1);
    if (tw2->tw_clock == 0)
	dmktime (tw2);

    return ((c1 = tw1->tw_clock) > (c2 = tw2->tw_clock) ? 1
	    : c1 == c2 ? 0 : -1);
}
