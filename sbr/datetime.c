/*
 * datetime.c -- functions for manipulating RFC 5545 date-time values
 *
 * This code is Copyright (c) 2014, by the authors of nmh.
 * See the COPYRIGHT file in the root directory of the nmh
 * distribution for complete copyright information.
 */

#include "h/mh.h"
#include "h/icalendar.h"
#include <h/fmt_scan.h>
#include "h/tws.h"
#include "h/utils.h"

/*
 * This doesn't try to support all of the myriad date-time formats
 * allowed by RFC 5545.  It is only used for viewing date-times,
 * so that shouldn't be a problem:  if a particular format can't
 * be handled by this code, just present it to the user in its
 * original form.
 *
 * And, this assumes a valid iCalendar input file.  E.g, it
 * doesn't check that each BEGIN has a matching END and vice
 * versa.  That should be done in the parser, though it currently
 * isn't.
 */

typedef struct tzparams {
    /* Pointers to values in parse tree.
     * TZOFFSETFROM is used to calculate the absolute time at which
     * the transition to a given observance takes place.
     * TZOFFSETTO is the timezone offset from UTC.  Both are in HHmm
     * format. */
    char *offsetfrom, *offsetto;
    const char *dtstart;
    const char *rrule;

    /* This is only used to make sure that timezone applies.  And not
       always, because if the timezone DTSTART is before the epoch, we
       don't try to compare to it. */
    time_t start_dt; /* in seconds since epoch */
} tzparams;

struct tzdesc {
    char *tzid;

    /* The following are translations of the pieces of RRULE and DTSTART
       into seconds from beginning of year. */
    tzparams standard_params;
    tzparams daylight_params;

    struct tzdesc *next;
};

/*
 * Parse a datetime of the form YYYYMMDDThhmmss and a string
 * representation of the timezone in units of [+-]hhmm and load the
 * struct tws.
 */
static int
parse_datetime (const char *datetime, const char *zone, int dst,
                struct tws *tws) {
    char utc_indicator;
    int form_1 = 0;
    int items_matched;

    memset(tws, 0, sizeof *tws);
    items_matched =
        sscanf (datetime, "%4d%2d%2dT%2d%2d%2d%c",
                &tws->tw_year, &tws->tw_mon, &tws->tw_mday,
                &tws->tw_hour, &tws->tw_min, &tws->tw_sec,
                &utc_indicator);
    tws->tw_flags = TW_NULL;

    if (items_matched == 7) {
        /* The 'Z' must be capital according to RFC 5545 Sec. 3.3.5. */
        if (utc_indicator != 'Z') {
            advise (NULL, "%s has invalid timezone indicator of 0x%x",
                    datetime, utc_indicator);
            return NOTOK;
        }
    } else if (zone == NULL) {
        form_1 = 1;
    }

    /* items_matched of 3 is for, e.g., 20151230.  Assume that means
       the entire day.  The time fields of the tws struct were
       initialized to 0 by the memset() above. */
    if (items_matched >= 6  ||  items_matched == 3) {
        int offset = atoi (zone ? zone : "0");

        /* struct tws defines tw_mon over [0, 11]. */
        --tws->tw_mon;

        /* Fill out rest of tws, i.e., its tw_wday and tw_flags. */
        set_dotw (tws);
        /* set_dotw() sets TW_SIMP.  Replace that with TW_SEXP so that
           dasctime() outputs the dotw before the date instead of after. */
        tws->tw_flags &= ~TW_SDAY, tws->tw_flags |= TW_SEXP;

        /* For the call to dmktime():
           - don't need tw_yday
           - tw_clock must be 0 on entry, and is set by dmktime()
           - the only flag in tw_flags used is TW_DST
         */
        tws->tw_yday = tws->tw_clock = 0;
        tws->tw_zone = 60 * (offset / 100)  +  offset % 100;
        if (dst) {
            tws->tw_zone -= 60;  /* per dlocaltime() */
            tws->tw_flags |= TW_DST;
        }
        /* dmktime() just sets tws->tw_clock. */
        (void) dmktime (tws);

        if (! form_1) {
            /* Set TW_SZEXP so that dasctime outputs timezone, except
               with local time (Form #1). */
            tws->tw_flags |= TW_SZEXP;

            /* Convert UTC time to time in local timezone.  However,
               don't try for years before 1970 because dlocatime()
               doesn't handle them well.  dlocaltime() will succeed if
               tws->tw_clock is nonzero. */
            if (tws->tw_year >= 1970  &&  tws->tw_clock > 0) {
                const int was_dst = tws->tw_flags & TW_DST;

                *tws = *dlocaltime (&tws->tw_clock);
                if (was_dst  &&  ! (tws->tw_flags & TW_DST)) {
                    /* dlocaltime() changed the DST flag from 1 to 0,
                       which means the time is in the hour (assumed to
                       be one hour) that is lost in the transition to
                       DST.  So per RFC 5545 Sec. 3.3.5, "the
                       DATE-TIME value is interpreted using the UTC
                       offset before the gap in local times."  In
                       other words, add an hour to it.
                       No adjustment is necessary for the transition
                       from DST to standard time, because dasctime()
                       shows the first occurrence of the time. */
                    tws->tw_clock += 3600;
                    *tws = *dlocaltime (&tws->tw_clock);
                }
            }
        }

        return OK;
    } else {
        return NOTOK;
    }
}

tzdesc_t
load_timezones (const contentline *clines) {
    tzdesc_t timezones = NULL, timezone = NULL;
    int in_vtimezone, in_standard, in_daylight;
    tzparams *params = NULL;
    const contentline *node;

    /* Interpret each VTIMEZONE section. */
    in_vtimezone = in_standard = in_daylight = 0;
    for (node = clines; node; node = node->next) {
        /* node->name will be NULL if the line was "deleted". */
        if (! node->name) { continue; }

        if (in_daylight  ||  in_standard) {
            if (! strcasecmp ("END", node->name)  &&
                ((in_standard  &&  ! strcasecmp ("STANDARD", node->value))  ||
                 (in_daylight  &&  ! strcasecmp ("DAYLIGHT", node->value)))) {
                struct tws tws;

                if (in_standard) { in_standard = 0; }
                else if (in_daylight) { in_daylight = 0; }
                if (parse_datetime (params->dtstart, params->offsetfrom,
                                    in_daylight ? 1 : 0,
                                    &tws) == OK) {
                    if (tws.tw_year >= 1970) {
                        /* dmktime() falls apart for, e.g., the year 1601. */
                        params->start_dt = tws.tw_clock;
                    }
                } else {
                    advise (NULL, "failed to parse start time %s for %s",
                            params->dtstart,
                            in_standard ? "standard" : "daylight");
                    return NULL;
                }
                params = NULL;
            } else if (! strcasecmp ("DTSTART", node->name)) {
                /* Save DTSTART for use after getting TZOFFSETFROM. */
                params->dtstart = node->value;
            } else if (! strcasecmp ("TZOFFSETFROM", node->name)) {
                params->offsetfrom = node->value;
            } else if (! strcasecmp ("TZOFFSETTO", node->name)) {
                params->offsetto = node->value;
            } else if (! strcasecmp ("RRULE", node->name)) {
                params->rrule = node->value;
            }
        } else if (in_vtimezone) {
            if (! strcasecmp ("END", node->name)  &&
                ! strcasecmp ("VTIMEZONE", node->value)) {
                in_vtimezone = 0;
            } else if (! strcasecmp ("BEGIN", node->name)  &&
                ! strcasecmp ("STANDARD", node->value)) {
                in_standard = 1;
                params = &timezone->standard_params;
            } else if (! strcasecmp ("BEGIN", node->name)  &&
                ! strcasecmp ("DAYLIGHT", node->value)) {
                in_daylight = 1;
                params = &timezone->daylight_params;
            } else if (! strcasecmp ("TZID", node->name)) {
                /* See comment below in format_datetime() about removing any enclosing quotes from a
                   timezone identifier. */
                char *buf = mh_xmalloc(strlen(node->value) + 1);
                unquote_string(node->value, buf);
                timezone->tzid = buf;
            }
        } else {
            if (! strcasecmp ("BEGIN", node->name)  &&
                ! strcasecmp ("VTIMEZONE", node->value)) {

                in_vtimezone = 1;
                NEW0(timezone);
                if (timezones) {
                    tzdesc_t t;

                    for (t = timezones; t && t->next; t = t->next) { continue; }
                    /* The loop terminated at, not after, the last
                       timezones node. */
                    t->next = timezone;
                } else {
                    timezones = timezone;
                }
            }
        }
    }

    return timezones;
}

void
free_timezones (tzdesc_t timezone) {
    tzdesc_t next;

    for ( ; timezone; timezone = next) {
        free (timezone->tzid);
        next = timezone->next;
        free (timezone);
    }
}

/*
 * Convert time to local timezone, accounting for daylight saving time:
 * - Detect which type of datetime the node contains:
 *     Form #1: DATE WITH LOCAL TIME
 *     Form #2: DATE WITH UTC TIME
 *     Form #3: DATE WITH LOCAL TIME AND TIME ZONE REFERENCE
 * - Convert value to local time in seconds since epoch.
 * - If there's a DST in the timezone, convert its start and end
 *   date-times to local time in seconds, also.  Then determine
 *   if the value is between them, and therefore DST.  Otherwise, it's
 *   not.
 * - Format the time value.
 */

/*
 * Given a recurrence rule and year, calculate its time in seconds
 * from 01 January UTC of the year.
 */
time_t
rrule_clock (const char *rrule, const char *starttime, const char *zone,
             unsigned int year) {
    time_t clock = 0;

    if (nmh_strcasestr (rrule, "FREQ=YEARLY;INTERVAL=1")  ||
        (nmh_strcasestr (rrule, "FREQ=YEARLY")  &&  nmh_strcasestr(rrule, "INTERVAL") == NULL)) {
        struct tws *tws;
        const char *cp;
        int wday = -1, month = -1;
        int specific_day = 1; /* BYDAY integer (prefix) */
        char buf[32];
        int day;

        if ((cp = nmh_strcasestr (rrule, "BYDAY="))) {
            cp += 6;
            /* BYDAY integers must be ASCII. */
            if (*cp == '+') { ++cp; } /* +n specific day; don't support '-' */
            else if (*cp == '-') { goto fail; }

            if (isdigit ((unsigned char) *cp)) { specific_day = *cp++ - 0x30; }

            if (! strncasecmp (cp, "SU", 2)) { wday = 0; }
            else if (! strncasecmp (cp, "MO", 2)) { wday = 1; }
            else if (! strncasecmp (cp, "TU", 2)) { wday = 2; }
            else if (! strncasecmp (cp, "WE", 2)) { wday = 3; }
            else if (! strncasecmp (cp, "TH", 2)) { wday = 4; }
            else if (! strncasecmp (cp, "FR", 2)) { wday = 5; }
            else if (! strncasecmp (cp, "SA", 2)) { wday = 6; }
        }
        if ((cp = nmh_strcasestr (rrule, "BYMONTH="))) {
            month = atoi (cp + 8);
        }

        for (day = 1; day <= 7; ++day) {
            /* E.g, 11-01-2014 02:00:00-0400 */
            snprintf (buf, sizeof buf, "%02d-%02d-%04u %.2s:%.2s:%.2s%s",
                      month, day + 7 * (specific_day-1), year,
                      starttime, starttime + 2, starttime + 4,
                      zone ? zone : "0000");
            if ((tws = dparsetime (buf))) {
                if (! (tws->tw_flags & (TW_SEXP|TW_SIMP))) { set_dotw (tws); }

                if (tws->tw_wday == wday) {
                    /* Found the day specified in the RRULE. */
                    break;
                }
            }
        }

        if (day <= 7) {
            clock = tws->tw_clock;
        }
    }

fail:
    if (clock == 0) {
        admonish (NULL,
                  "Unsupported RRULE format: %s, assume local timezone",
                  rrule);
    }

    return clock;
}

char *
format_datetime (tzdesc_t timezones, const contentline *node) {
    param_list *p;
    char *dt_timezone = NULL;
    int dst = 0;
    struct tws tws[2]; /* [standard, daylight] */
    tzdesc_t tz;
    char *tp_std, *tp_dst, *tp_dt;

    /* Extract the timezone, if specified (RFC 5545 Sec. 3.3.5 Form #3). */
    for (p = node->params; p && p->param_name; p = p->next) {
        if (! strcasecmp (p->param_name, "TZID")  &&  p->values) {
            /* Remove any enclosing quotes from the timezone identifier.  I don't believe that it's
               legal for it to be quoted, according to RFC 5545 § 3.2.19:
                   tzidparam  = "TZID" "=" [tzidprefix] paramtext
                   tzidprefix = "/"
               where paramtext includes SAFE-CHAR, which specifically excludes DQUOTE.  But we'll
               be generous and strip quotes. */
            char *buf = mh_xmalloc(strlen(p->values->value) + 1);
            unquote_string(p->values->value, buf);
            dt_timezone = buf;
            break;
        }
    }

    if (! dt_timezone) {
        /* Form #1: DATE WITH LOCAL TIME, i.e., no time zone, or
           Form #2: DATE WITH UTC TIME */
        if (parse_datetime (node->value, NULL, 0, &tws[0]) == OK) {
            return strdup (dasctime (&tws[0], 0));
        } else {
            advise (NULL, "unable to parse datetime %s", node->value);
            return NULL;
        }
    }

    /*
     * must be
     * Form #3: DATE WITH LOCAL TIME AND TIME ZONE REFERENCE
     */

    /* Find the corresponding tzdesc. */
    for (tz = timezones; dt_timezone && tz; tz = tz->next) {
        /* Property parameter values are case insenstive (RFC 5545
           Sec. 2) and time zone identifiers are property parameters
           (RFC 5545 Sec. 3.8.2.4), though it would seem odd to use
           different case in the same file for identifiers that are
           supposed to be the same. */
        if (tz->tzid  &&  ! strcasecmp (dt_timezone, tz->tzid)) { break; }
    }

    if (tz) {
        free(dt_timezone);
    } else {
        advise (NULL, "did not find VTIMEZONE section for %s", dt_timezone);
        free(dt_timezone);
        return NULL;
    }

    /* Determine if it's Daylight Saving. */
    tp_std = strchr (tz->standard_params.dtstart, 'T');
    tp_dt = strchr (node->value, 'T');

    if (tz->daylight_params.dtstart) {
        tp_dst = strchr (tz->daylight_params.dtstart, 'T');
    } else {
        /* No DAYLIGHT section. */
        tp_dst = NULL;
        dst = 0;
    }

    if (tp_std  &&  tp_dt) {
        time_t transition[2] = { 0, 0 }; /* [standard, daylight] */
        time_t dt[2]; /* [standard, daylight] */
        unsigned int year;
        char buf[5];

        /* Datetime is form YYYYMMDDThhmmss.  Extract year. */
        memcpy (buf, node->value, sizeof buf - 1);
        buf[sizeof buf - 1] = '\0';
        year = atoi (buf);

        if (tz->standard_params.rrule) {
            /* +1 to skip the T before the time */
            transition[0] =
                rrule_clock (tz->standard_params.rrule, tp_std + 1,
                             tz->standard_params.offsetfrom, year);
        }
        if (tp_dst  &&  tz->daylight_params.rrule) {
            /* +1 to skip the T before the time */
            transition[1] =
                rrule_clock (tz->daylight_params.rrule, tp_dst + 1,
                             tz->daylight_params.offsetfrom, year);
        }

        if (transition[0] < transition[1]) {
            advise (NULL, "format_datetime() requires that daylight "
                    "saving time transition precede standard time "
                    "transition");
            return NULL;
        }

        if (parse_datetime (node->value, tz->standard_params.offsetto,
                            0, &tws[0]) == OK) {
            dt[0] = tws[0].tw_clock;
        } else {
            advise (NULL, "unable to parse datetime %s", node->value);
            return NULL;
        }

        if (tp_dst) {
            if (dt[0] < transition[1]) {
                dst = 0;
            } else {
                if (parse_datetime (node->value,
                                    tz->daylight_params.offsetto, 1,
                                    &tws[1]) == OK) {
                    dt[1] = tws[1].tw_clock;
                } else {
                    advise (NULL, "unable to parse datetime %s",
                            node->value);
                    return NULL;
                }

                dst = dt[1] > transition[0]  ?  0  :  1;
            }
        }

        if (dst) {
            if (tz->daylight_params.start_dt > 0  &&
                dt[dst] < tz->daylight_params.start_dt) {
                advise (NULL, "date-time of %s is before VTIMEZONE start "
                        "of %s", node->value,
                        tz->daylight_params.dtstart);
                return NULL;
            }
        } else {
            if (tz->standard_params.start_dt > 0  &&
                dt[dst] < tz->standard_params.start_dt) {
                advise (NULL, "date-time of %s is before VTIMEZONE start "
                        "of %s", node->value,
                        tz->standard_params.dtstart);
                return NULL;
            }
        }
    } else {
        if (! tp_std) {
            advise (NULL, "unsupported date-time format: %s",
                    tz->standard_params.dtstart);
            return NULL;
        }
        if (! tp_dt) {
            advise (NULL, "unsupported date-time format: %s", node->value);
            return NULL;
        }
    }

    return strdup (dasctime (&tws[dst], 0));
}
