/* tws.h -- time routines.
 */

/* A timezone given as a numeric-only offset will
   be treated specially if it's in a zone that observes Daylight Saving Time.
   For instance, during DST, a Date: like "Mon, 24 Jul 2000 12:31:44 -0700" will
   be printed as "Mon, 24 Jul 2000 12:31:44 PDT".  Without the code activated by
   the following #define, that would be incorrectly printed as "...MST". */

struct tws {
    int tw_sec;		/* seconds after the minute - [0, 61] */
    int tw_min;		/* minutes after the hour - [0, 59]   */
    int tw_hour;	/* hour since midnight - [0, 23]      */
    int tw_mday;	/* day of the month - [1, 31]         */
    int tw_mon;		/* months since January - [0, 11]     */
    int tw_year;	/* 4 digit year (e.g. 1997)           */
    int tw_wday;	/* days since Sunday - [0, 6]         */
    int tw_yday;	/* days since January 1 - [0, 365]    */
    int tw_zone;
    time_t tw_clock;	/* if != 0, corresponding calendar value */
    int tw_flags;
};

#define	TW_NULL	 0x0000

#define	TW_SDAY	 0x0003	/* how day-of-week was determined */
#define	TW_SEXP	 0x0001	/*   explicitly given             */
#define	TW_SIMP	 0x0002	/*   implicitly given             */

#define	TW_SZEXP 0x0004	/* Explicit timezone. */

#define	TW_DST	 0x0010	/* daylight savings time          */
#define	TW_ZONE	 0x0020	/* use numeric timezones only     */

#define TW_SUCC  0x0040 /* Parsing was successful. */

#define	dtwszone(tw) dtimezone (tw->tw_zone, tw->tw_flags)

/*
 * prototypes
 */
struct tws *dparsetime (char *);
