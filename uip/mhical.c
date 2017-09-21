/* mhical.c -- operate on an iCalendar request
 *
 * This code is Copyright (c) 2014, by the authors of nmh.
 * See the COPYRIGHT file in the root directory of the nmh
 * distribution for complete copyright information.
 */

#include "h/mh.h"
#include "h/icalendar.h"
#include "sbr/icalparse.h"
#include <h/fmt_scan.h>
#include "h/addrsbr.h"
#include "h/mts.h"
#include "h/done.h"
#include "h/utils.h"
#include <time.h>

typedef enum act {
    ACT_NONE,
    ACT_ACCEPT,
    ACE_DECLINE,
    ACT_TENTATIVE,
    ACT_DELEGATE,
    ACT_CANCEL
} act;

static void convert_to_reply (contentline *, act);
static void convert_to_cancellation (contentline *);
static void convert_common (contentline *, act);
static void dump_unfolded (FILE *, contentline *);
static void output (FILE *, contentline *, int);
static void display (FILE *, contentline *, char *);
static const char *identity (const contentline *) PURE;
static char *format_params (char *, param_list *);
static char *fold (char *, int);

#define MHICAL_SWITCHES \
    X("reply accept|decline|tentative", 0, REPLYSW) \
    X("cancel", 0, CANCELSW) \
    X("form formatfile", 0, FORMSW) \
    X("format string", 5, FMTSW) \
    X("infile", 0, INFILESW) \
    X("outfile", 0, OUTFILESW) \
    X("contenttype", 0, CONTENTTYPESW) \
    X("nocontenttype", 0, NCONTENTTYPESW) \
    X("unfold", 0, UNFOLDSW) \
    X("debug", 0, DEBUGSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(MHICAL);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(MHICAL, switches);
#undef X

vevent vevents = { NULL, NULL, NULL};
int parser_status = 0;

int
main (int argc, char *argv[]) {
    /* RFC 5322 § 3.3 date-time format, including the optional
       day-of-week and not including the optional seconds.  The
       zone is required by the RFC but not always output by this
       format, because RFC 5545 § 3.3.5 allows date-times not
       bound to any time zone. */

    act action = ACT_NONE;
    char *infile = NULL, *outfile = NULL;
    FILE *inputfile = NULL, *outputfile = NULL;
    int contenttype = 0, unfold = 0;
    vevent *v, *nextvevent;
    char *form = "mhical.24hour", *format = NULL;
    char **argp, **arguments, *cp;

    icaldebug = 0;  /* Global provided by bison (with name-prefix "ical"). */

    if (nmh_init(argv[0], 2)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /*
     * Parse arguments
     */
    while ((cp = *argp++)) {
        if (*cp == '-') {
            switch (smatch (++cp, switches)) {
            case AMBIGSW:
                ambigsw (cp, switches);
                done (1);
            case UNKWNSW:
                adios (NULL, "-%s unknown", cp);

            case HELPSW: {
                char buf[128];
                snprintf (buf, sizeof buf, "%s [switches]", invo_name);
                print_help (buf, switches, 1);
                done (0);
            }
            case VERSIONSW:
                print_version(invo_name);
                done (0);
            case DEBUGSW:
                icaldebug = 1;
                continue;

            case REPLYSW:
                if (! (cp = *argp++) || (*cp == '-' && cp[1]))
                    adios (NULL, "missing argument to %s", argp[-2]);
                if (! strcasecmp (cp, "accept")) {
                    action = ACT_ACCEPT;
                } else if (! strcasecmp (cp, "decline")) {
                    action = ACE_DECLINE;
                } else if (! strcasecmp (cp, "tentative")) {
                    action = ACT_TENTATIVE;
                } else if (! strcasecmp (cp, "delegate")) {
                    action = ACT_DELEGATE;
                } else {
                    adios (NULL, "Unknown action: %s", cp);
                }
                continue;

            case CANCELSW:
                action = ACT_CANCEL;
                continue;

            case FORMSW:
                if (! (form = *argp++) || *form == '-')
                    adios (NULL, "missing argument to %s", argp[-2]);
                format = NULL;
                continue;
            case FMTSW:
                if (! (format = *argp++) || *format == '-')
                    adios (NULL, "missing argument to %s", argp[-2]);
                form = NULL;
                continue;

            case INFILESW:
                if (! (cp = *argp++) || (*cp == '-' && cp[1]))
                    adios (NULL, "missing argument to %s", argp[-2]);
                infile = *cp == '-'  ?  mh_xstrdup(cp)  :  path (cp, TFILE);
                continue;
            case OUTFILESW:
                if (! (cp = *argp++) || (*cp == '-' && cp[1]))
                    adios (NULL, "missing argument to %s", argp[-2]);
                outfile = *cp == '-'  ?  mh_xstrdup(cp)  :  path (cp, TFILE);
                continue;

            case CONTENTTYPESW:
                contenttype = 1;
                continue;
            case NCONTENTTYPESW:
                contenttype = 0;
                continue;

            case UNFOLDSW:
                unfold = 1;
                continue;
            }
        }
    }

    free (arguments);

    if (infile) {
        if ((inputfile = fopen (infile, "r"))) {
            icalset_inputfile (inputfile);
        } else {
            adios (infile, "error opening");
        }
    } else {
        inputfile = stdin;
    }

    if (outfile) {
        if ((outputfile = fopen (outfile, "w"))) {
            icalset_outputfile (outputfile);
        } else {
            adios (outfile, "error opening");
        }
    } else {
        outputfile = stdout;
    }

    vevents.last = &vevents;
    /* vevents is accessed by parser as global. */
    icalparse ();

    for (v = &vevents; v; v = nextvevent) {
        if (! unfold  &&  v != &vevents  &&  v->contentlines  &&
            v->contentlines->name  &&
            strcasecmp (v->contentlines->name, "END")  &&
            v->contentlines->value  &&
            strcasecmp (v->contentlines->value, "VCALENDAR")) {
            /* Output blank line between vevents.  Not before
               first vevent and not after last. */
            putc ('\n', outputfile);
        }

        if (action == ACT_NONE) {
            if (unfold) {
                dump_unfolded (outputfile, v->contentlines);
            } else {
                char *nfs = new_fs (form, format, NULL);

                display (outputfile, v->contentlines, nfs);
                free_fs ();
            }
        } else {
            if (action == ACT_CANCEL) {
                convert_to_cancellation (v->contentlines);
            } else {
                convert_to_reply (v->contentlines, action);
            }
            output (outputfile, v->contentlines, contenttype);
        }

        free_contentlines (v->contentlines);
        nextvevent = v->next;
        if (v != &vevents) {
            free (v);
        }
    }

    if (infile) {
        if (fclose (inputfile) != 0) {
            advise (infile, "error closing");
        }
        free (infile);
    }
    if (outfile) {
        if (fclose (outputfile) != 0) {
            advise (outfile, "error closing");
        }
        free (outfile);
    }

    return parser_status;
}

/*
 * - Change METHOD from REQUEST to REPLY.
 * - Change PRODID.
 * - Remove all ATTENDEE lines for other users (based on ismymbox ()).
 * - For the user's ATTENDEE line:
 *   - Remove ROLE and RSVP parameters.
 *   - Change PARTSTAT value to indicate reply action, e.g., ACCEPTED,
 *     DECLINED, or TENTATIVE.
 * - Insert action at beginning of SUMMARY value.
 * - Remove all X- lines.
 * - Update DTSTAMP with current timestamp.
 * - Remove all DESCRIPTION lines.
 * - Excise VALARM sections.
 */
static void
convert_to_reply (contentline *clines, act action) {
    char *partstat = NULL;
    int found_my_attendee_line = 0;
    contentline *node;

    convert_common (clines, action);

    switch (action) {
    case ACT_ACCEPT:
        partstat = "ACCEPTED";
        break;
    case ACE_DECLINE:
        partstat = "DECLINED";
        break;
    case ACT_TENTATIVE:
        partstat = "TENTATIVE";
        break;
    default:
        ;
    }

    /* Call find_contentline () with node as argument to find multiple
       matching contentlines. */
    for (node = clines;
         (node = find_contentline (node, "ATTENDEE", 0));
         node = node->next) {
        param_list *p;

        ismymbox (NULL); /* need to prime ismymbox() */

        /* According to RFC 5545 § 3.3.3, an email address in the
           value must be a mailto URI. */
        if (! strncasecmp (node->value, "mailto:", 7)) {
            char *addr = node->value + 7;
            struct mailname *mn;

            /* Skip any leading whitespace. */
            for ( ; isspace ((unsigned char) *addr); ++addr) { continue; }

            addr = getname (addr);
            mn = getm (addr, NULL, 0, NULL, 0);

            /* Need to flush getname after use. */
            while (getname ("")) { continue; }

            if (ismymbox (mn)) {
                found_my_attendee_line = 1;
                for (p = node->params; p && p->param_name; p = p->next) {
                    value_list *v;

                    for (v = p->values; v; v = v->next) {
                        if (! strcasecmp (p->param_name, "ROLE")  ||
                            ! strcasecmp (p->param_name, "RSVP")) {
                            remove_value (v);
                        } else if (! strcasecmp (p->param_name, "PARTSTAT")) {
                            free (v->value);
                            v->value = strdup (partstat);
                        }
                    }
                }
            } else {
                remove_contentline (node);
            }

            mnfree (mn);
        }
    }

    if (! found_my_attendee_line) {
        /* Generate and attach an ATTENDEE line for me. */
        contentline *node;

        /* Add it after the ORGANIZER line, or if none, BEGIN:VEVENT line. */
        if ((node = find_contentline (clines, "ORGANIZER", 0))  ||
            (node = find_contentline (clines, "BEGIN", "VEVENT"))) {
            contentline *new_node = add_contentline (node, "ATTENDEE");

            add_param_name (new_node, mh_xstrdup ("PARTSTAT"));
            add_param_value (new_node, mh_xstrdup (partstat));
            add_param_name (new_node, mh_xstrdup ("CN"));
            add_param_value (new_node, mh_xstrdup (getfullname ()));
            new_node->value = concat ("MAILTO:", getlocalmbox (), NULL);
        }
    }

    /* Call find_contentline () with node as argument to find multiple
       matching contentlines. */
    for (node = clines;
         (node = find_contentline (node, "DESCRIPTION", 0));
         node = node->next) {
        /* ACCEPT, at least, replies don't seem to have DESCRIPTIONS. */
        remove_contentline (node);
    }
}

/*
 * - Change METHOD from REQUEST to CANCEL.
 * - Change PRODID.
 * - Insert action at beginning of SUMMARY value.
 * - Remove all X- lines.
 * - Update DTSTAMP with current timestamp.
 * - Change STATUS from CONFIRMED to CANCELLED.
 * - Increment value of SEQUENCE.
 * - Excise VALARM sections.
 */
static void
convert_to_cancellation (contentline *clines) {
    contentline *node;

    convert_common (clines, ACT_CANCEL);

    if ((node = find_contentline (clines, "STATUS", 0))  &&
        ! strcasecmp (node->value, "CONFIRMED")) {
        free (node->value);
        node->value = mh_xstrdup ("CANCELLED");
    }

    if ((node = find_contentline (clines, "SEQUENCE", 0))) {
        int sequence = atoi (node->value);
        char buf[32];

        (void) snprintf (buf, sizeof buf, "%d", sequence + 1);
        free (node->value);
        node->value = mh_xstrdup (buf);
    }
}

static void
convert_common (contentline *clines, act action) {
    contentline *node;
    int in_valarm;

    if ((node = find_contentline (clines, "METHOD", 0))) {
        free (node->value);
        node->value = mh_xstrdup (action == ACT_CANCEL  ?  "CANCEL"  :  "REPLY");
    }

    if ((node = find_contentline (clines, "PRODID", 0))) {
        free (node->value);
        node->value = mh_xstrdup ("nmh mhical v0.1");
    }

    if ((node = find_contentline (clines, "VERSION", 0))) {
        if (! node->value) {
            inform("Version property is missing value, assume 2.0, continuing...");
            node->value = mh_xstrdup ("2.0");
        }

        if (strcmp (node->value, "2.0")) {
            inform("supports the Version 2.0 specified by RFC 5545 "
		"but iCalendar object has Version %s, continuing...",
		node->value);
            node->value = mh_xstrdup ("2.0");
        }
    }

    if ((node = find_contentline (clines, "SUMMARY", 0))) {
        char *insert = NULL;

        switch (action) {
        case ACT_ACCEPT:
            insert = "Accepted: ";
            break;
        case ACE_DECLINE:
            insert = "Declined: ";
            break;
        case ACT_TENTATIVE:
            insert = "Tentative: ";
            break;
        case ACT_DELEGATE:
            adios (NULL, "Delegate replies are not supported");
            break;
        case ACT_CANCEL:
            insert = "Cancelled:";
            break;
        default:
            ;
        }

        if (insert) {
            const size_t len = strlen (insert) + strlen (node->value) + 1;
            char *tmp = mh_xmalloc (len);

            (void) strncpy (tmp, insert, len);
            (void) strncat (tmp, node->value, len - strlen (insert) - 1);
            free (node->value);
            node->value = tmp;
        } else {
            /* Should never get here. */
            adios (NULL, "Unknown action: %d", action);
        }
    }

    if ((node = find_contentline (clines, "DTSTAMP", 0))) {
        const time_t now = time (NULL);
        struct tm now_tm;

        if (gmtime_r (&now, &now_tm)) {
            /* 17 would be sufficient given that RFC 5545 § 3.3.4
               supports only a 4 digit year. */
            char buf[32];

            if (strftime (buf, sizeof buf, "%Y%m%dT%H%M%SZ", &now_tm)) {
                free (node->value);
                node->value = mh_xstrdup (buf);
            } else {
                inform("strftime unable to format current time, continuing...");
            }
        } else {
            inform("gmtime_r failed on current time, continuing...");
        }
    }

    /* Excise X- lines and VALARM section(s). */
    in_valarm = 0;
    for (node = clines; node; node = node->next) {
        /* node->name will be NULL if the line was deleted. */
        if (! node->name) { continue; }

        if (in_valarm) {
            if (! strcasecmp ("END", node->name)  &&
                ! strcasecmp ("VALARM", node->value)) {
                in_valarm = 0;
            }
            remove_contentline (node);
        } else {
            if (! strcasecmp ("BEGIN", node->name)  &&
                ! strcasecmp ("VALARM", node->value)) {
                in_valarm = 1;
                remove_contentline (node);
            } else if (! strncasecmp ("X-", node->name, 2)) {
                remove_contentline (node);
            }
        }
    }
}

/* Echo the input, but with unfolded lines. */
static void
dump_unfolded (FILE *file, contentline *clines) {
    contentline *node;

    for (node = clines; node; node = node->next) {
        fputs (node->input_line, file);
    }
}

static void
output (FILE *file, contentline *clines, int contenttype) {
    contentline *node;

    if (contenttype) {
        /* Generate a Content-Type header to pass the method parameter
           to mhbuild.  Per RFC 5545 Secs. 6 and 8.1, it must be
           UTF-8.  But we don't attempt to do any conversion of the
           input. */
        if ((node = find_contentline (clines, "METHOD", 0))) {
            fprintf (file,
                     "Content-Type: text/calendar; method=\"%s\"; "
                     "charset=\"UTF-8\"\n\n",
                     node->value);
        }
    }

    for (node = clines; node; node = node->next) {
        if (node->name) {
            char *line = NULL;
            size_t len;

            line = mh_xstrdup (node->name);
            line = format_params (line, node->params);

            len = strlen (line);
            line = mh_xrealloc (line, len + 2);
            line[len] = ':';
            line[len + 1] = '\0';

            line = fold (add (node->value, line),
                         clines->cr_before_lf == CR_BEFORE_LF);

            fputs(line, file);
            if (clines->cr_before_lf != LF_ONLY)
                putc('\r', file);
            putc('\n', file);
            free (line);
        }
    }
}

/*
 * Display these fields of the iCalendar event:
 *   - method
 *   - organizer
 *   - summary
 *   - description, except for "\n\n" and in VALARM
 *   - location
 *   - dtstart in local timezone
 *   - dtend in local timezone
 *   - attendees (limited to number specified in initialization)
 */
static void
display (FILE *file, contentline *clines, char *nfs) {
    tzdesc_t timezones = load_timezones (clines);
    int in_vtimezone;
    int in_valarm;
    contentline *node;
    struct format *fmt;
    int dat[5] = { 0, 0, 0, INT_MAX, 0 };
    struct comp *c;
    charstring_t buffer = charstring_create (BUFSIZ);
    charstring_t attendees = charstring_create (BUFSIZ);
    const unsigned int max_attendees = 20;
    unsigned int num_attendees;

    /* Don't call on the END:VCALENDAR line. */
    if (clines  &&  clines->next) {
      (void) fmt_compile (nfs, &fmt, 1);
    }

    if ((c = fmt_findcomp ("method"))) {
        if ((node = find_contentline (clines, "METHOD", 0))  &&  node->value) {
            c->c_text = mh_xstrdup (node->value);
        }
    }

    if ((c = fmt_findcomp ("organizer"))) {
        if ((node = find_contentline (clines, "ORGANIZER", 0))  &&
            node->value) {
            c->c_text = mh_xstrdup (identity (node));
        }
    }

    if ((c = fmt_findcomp ("summary"))) {
        if ((node = find_contentline (clines, "SUMMARY", 0))  &&  node->value) {
            c->c_text = mh_xstrdup (node->value);
        }
    }

    /* Only display DESCRIPTION lines that are outside VALARM section(s). */
    in_valarm = 0;
    if ((c = fmt_findcomp ("description"))) {
        for (node = clines; node; node = node->next) {
            /* node->name will be NULL if the line was deleted. */
            if (node->name  &&  node->value  &&  ! in_valarm  &&
                ! strcasecmp ("DESCRIPTION", node->name)  &&
                strcasecmp (node->value, "\\n\\n")) {
                c->c_text = mh_xstrdup (node->value);
            } else if (in_valarm) {
                if (! strcasecmp ("END", node->name)  &&
                    ! strcasecmp ("VALARM", node->value)) {
                    in_valarm = 0;
                }
            } else {
                if (! strcasecmp ("BEGIN", node->name)  &&
                    ! strcasecmp ("VALARM", node->value)) {
                    in_valarm = 1;
                }
            }
        }
    }

    if ((c = fmt_findcomp ("location"))) {
        if ((node = find_contentline (clines, "LOCATION", 0))  &&
            node->value) {
            c->c_text = mh_xstrdup (node->value);
        }
    }

    if ((c = fmt_findcomp ("dtstart"))) {
        /* Find DTSTART outsize of a VTIMEZONE section. */
        in_vtimezone = 0;
        for (node = clines; node; node = node->next) {
            /* node->name will be NULL if the line was deleted. */
            if (! node->name) { continue; }

            if (in_vtimezone) {
                if (! strcasecmp ("END", node->name)  &&
                    ! strcasecmp ("VTIMEZONE", node->value)) {
                    in_vtimezone = 0;
                }
            } else {
                if (! strcasecmp ("BEGIN", node->name)  &&
                    ! strcasecmp ("VTIMEZONE", node->value)) {
                    in_vtimezone = 1;
                } else if (! strcasecmp ("DTSTART", node->name)) {
                    /* Got it:  DTSTART outside of a VTIMEZONE section. */
                    char *datetime = format_datetime (timezones, node);
                    c->c_text = datetime ? datetime : mh_xstrdup(node->value);
                }
            }
        }
    }

    if ((c = fmt_findcomp ("dtend"))) {
        if ((node = find_contentline (clines, "DTEND", 0))  &&  node->value) {
            char *datetime = format_datetime (timezones, node);
            c->c_text = datetime ? datetime : strdup(node->value);
        } else if ((node = find_contentline (clines, "DTSTART", 0))  &&
                   node->value) {
            /* There is no DTEND.  If there's a DTSTART, use it.  If it
               doesn't have a time, assume that the event is for the
               entire day and append 23:59:59 to it so that it signifies
               the end of the day.  And assume local timezone. */
            if (strchr(node->value, 'T')) {
                char * datetime = format_datetime (timezones, node);
                c->c_text = datetime ? datetime : strdup(node->value);
            } else {
                char *datetime;
                contentline node_copy;

                node_copy = *node;
                node_copy.value = concat(node_copy.value, "T235959", NULL);
                datetime = format_datetime (timezones, &node_copy);
                c->c_text = datetime ? datetime : strdup(node_copy.value);
                free(node_copy.value);
            }
        }
    }

    if ((c = fmt_findcomp ("attendees"))) {
        /* Call find_contentline () with node as argument to find multiple
           matching contentlines. */
        charstring_append_cstring (attendees, "Attendees: ");
        for (node = clines, num_attendees = 0;
             (node = find_contentline (node, "ATTENDEE", 0))  &&
                 num_attendees++ < max_attendees;
             node = node->next) {
            const char *id = identity (node);

            if (num_attendees > 1) {
                charstring_append_cstring (attendees, ", ");
            }
            charstring_append_cstring (attendees, id);
        }

        if (num_attendees >= max_attendees) {
            unsigned int not_shown = 0;

            for ( ;
                  (node = find_contentline (node, "ATTENDEE", 0));
                  node = node->next) {
                ++not_shown;
            }

            if (not_shown > 0) {
                char buf[32];

                (void) snprintf (buf, sizeof buf, ", and %d more", not_shown);
                charstring_append_cstring (attendees, buf);
            }
        }

        if (num_attendees > 0) {
            c->c_text = charstring_buffer_copy (attendees);
        }
    }

    /* Don't call on the END:VCALENDAR line. */
    if (clines  &&  clines->next) {
      (void) fmt_scan (fmt, buffer, INT_MAX, dat, NULL);
      fputs (charstring_buffer (buffer), file);
      fmt_free (fmt, 1);
    }

    charstring_free (attendees);
    charstring_free (buffer);
    free_timezones (timezones);
}

static const char *
identity (const contentline *node) {
    /* According to RFC 5545 § 3.3.3, an email address in the value
       must be a mailto URI. */
    if (! strncasecmp (node->value, "mailto:", 7)) {
        char *addr;
        param_list *p;

        for (p = node->params; p && p->param_name; p = p->next) {
            value_list *v;

            for (v = p->values; v; v = v->next) {
                if (! strcasecmp (p->param_name, "CN")) {
                    return v->value;
                }
            }
        }

        /* Did not find a CN parameter, so output the address. */
        addr = node->value + 7;

        /* Skip any leading whitespace. */
        for ( ; isspace ((unsigned char) *addr); ++addr) { continue; }

        return addr;
    }

    return "unknown";
}

static char *
format_params (char *line, param_list *p) {
    for ( ; p && p->param_name; p = p->next) {
        value_list *v;
        size_t num_values = 0;

        for (v = p->values; v; v = v->next) {
            if (v->value) { ++num_values; }
        }

        if (num_values) {
            size_t len = strlen (line);

            line = mh_xrealloc (line, len + 2);
            line[len] = ';';
            line[len + 1] = '\0';

            line = add (p->param_name, line);

            for (v = p->values; v; v = v->next) {
                len = strlen (line);
                line = mh_xrealloc (line, len + 2);
                line[len] = v == p->values ? '=' : ',';
                line[len + 1] = '\0';

                line = add (v->value, line);
            }
        }
    }

    return line;
}

static char *
fold (char *line, int uses_cr) {
    size_t remaining = strlen (line);
    size_t current_line_len = 0;
    charstring_t folded_line = charstring_create (2 * remaining);
    const char *cp = line;

#ifdef MULTIBYTE_SUPPORT
    if (mbtowc (NULL, NULL, 0)) {} /* reset shift state */
#endif

    while (*cp  &&  remaining > 0) {
#ifdef MULTIBYTE_SUPPORT
        int char_len = mbtowc (NULL, cp, (size_t) MB_CUR_MAX < remaining
                                         ? (size_t) MB_CUR_MAX
                                         : remaining);
        if (char_len == -1) { char_len = 1; }
#else
        const int char_len = 1;
#endif

        charstring_push_back_chars (folded_line, cp, char_len, 1);
        remaining -= max(char_len, 1);

        /* remaining must be > 0 to pass the loop condition above, so
           if it's not > 1, it is == 1. */
        if (++current_line_len >= 75) {
            if (remaining > 1  ||  (*(cp+1) != '\0'  &&  *(cp+1) != '\r'  &&
                                    *(cp+1) != '\n')) {
                /* fold */
                if (uses_cr) { charstring_push_back (folded_line, '\r'); }
                charstring_push_back (folded_line, '\n');
                charstring_push_back (folded_line, ' ');
                current_line_len = 0;
            }
        }

        cp += max(char_len, 1);
    }

    free (line);
    line = charstring_buffer_copy (folded_line);
    charstring_free (folded_line);

    return line;
}
