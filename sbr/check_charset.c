/* check_charset.c -- routines for character sets
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "check_charset.h"

#include <string.h>
#include <langinfo.h>

static const char *norm_charmap(char *name);

/*
 * Get the current character set
 */
char *
get_charset(void)
{
    return (char *)norm_charmap(nl_langinfo(CODESET));
}


/*
 * Check if we can display a given character set natively.
 * We are passed the length of the initial part of the
 * string to check, since we want to allow the name of the
 * character set to be a substring of a larger string.
 */

int
check_charset (char *str, int len) 
{
    static char *mm_charset = NULL;
    static char *alt_charset = NULL;
    static int mm_len;
    static int alt_len;

    /* Cache the name of our default character set */
    if (!mm_charset) {
	if (!(mm_charset = get_charset ()))
	    mm_charset = "US-ASCII";
	mm_len = strlen (mm_charset);

	/* US-ASCII is a subset of the ISO-8859-X and UTF-8 character sets */
	if (!strncasecmp("ISO-8859-", mm_charset, 9) ||
		!strcasecmp("UTF-8", mm_charset)) {
	    alt_charset = "US-ASCII";
	    alt_len = strlen (alt_charset);
	}
    }

    /* Check if character set is OK */
    if ((len == mm_len) && !strncasecmp(str, mm_charset, mm_len))
	return 1;
    if (alt_charset && (len == alt_len) && !strncasecmp(str, alt_charset, alt_len))
	return 1;

    return 0;
}


/*
 * Return the name of the character set we are
 * using for 8bit text.
 */
char *
write_charset_8bit (void)
{
    static char *mm_charset = NULL;

    /*
     * Cache the name of the character set to
     * use for 8bit text.
     */
    if (!mm_charset && !(mm_charset = get_charset ()))
	    mm_charset = "x-unknown";

    return mm_charset;
}

/* The Single Unix Specification function nl_langinfo(CODESET)
 * returns the name of the encoding used by the currently selected
 * locale:
 *
 *   http://www.opengroup.org/onlinepubs/7908799/xsh/langinfo.h.html
 *
 * Unfortunately the encoding names are not yet standardized.
 * This function knows about the encoding names used on many
 * different systems and converts them where possible into
 * the corresponding MIME charset name registered in
 *
 *   http://www.iana.org/assignments/character-sets
 *
 * Please extend it as needed and suggest improvements to the author.
 *
 * Markus.Kuhn@cl.cam.ac.uk -- 2002-03-11
 * Permission to use, copy, modify, and distribute this software
 * for any purpose and without fee is hereby granted. The author
 * disclaims all warranties with regard to this software.
 *
 * Latest version:
 *
 *   http://www.cl.cam.ac.uk/~mgk25/ucs/norm_charmap.c
 */

static const char *
norm_charmap(char *name)
{
    static const char *correct[] = {
        "UTF-8",
        "US-ASCII",
        NULL
    }, **cor;
    static struct {
        const char *alias;
        const char *name;
    } *ali, aliases[] = {
        /* Names for US-ASCII. */
        { "ANSI_X3.4-1968", "US-ASCII" }, /* LC_ALL=C. */
        { "ASCII", "US-ASCII" },
        { "646", "US-ASCII" },
        { "ISO646", "US-ASCII" },
        { "ISO_646.IRV", "US-ASCII" },
        /* Case differs. */
        { "BIG5", "Big5" },
        { "BIG5HKSCS", "Big5HKSCS" },
        /* Names for ISO-8859-11. */
        { "TIS-620", "ISO-8859-11" },
        { "TIS620.2533", "ISO-8859-11" },
        { NULL, NULL }
    };
    static struct {
        const char *substr;
        const char *name;
    } *sub, substrs[] = {
        { "8859-1", "ISO-8859-1" },
        { "8859-2", "ISO-8859-2" },
        { "8859-3", "ISO-8859-3" },
        { "8859-4", "ISO-8859-4" },
        { "8859-5", "ISO-8859-5" },
        { "8859-6", "ISO-8859-6" },
        { "8859-7", "ISO-8859-7" },
        { "8859-8", "ISO-8859-8" },
        { "8859-9", "ISO-8859-9" },
        { "8859-10", "ISO-8859-10" },
        { "8859-11", "ISO-8859-11" },
        /* 12, Latin/Devanagari, not completed. */
        { "8859-13", "ISO-8859-13" },
        { "8859-14", "ISO-8859-14" },
        { "8859-15", "ISO-8859-15" },
        { "8859-16", "ISO-8859-16" },
        { "CP1200", "WINDOWS-1200" },
        { "CP1201", "WINDOWS-1201" },
        { "CP1250", "WINDOWS-1250" },
        { "CP1251", "WINDOWS-1251" },
        { "CP1252", "WINDOWS-1252" },
        { "CP1253", "WINDOWS-1253" },
        { "CP1254", "WINDOWS-1254" },
        { "CP1255", "WINDOWS-1255" },
        { "CP1256", "WINDOWS-1256" },
        { "CP1257", "WINDOWS-1257" },
        { "CP1258", "WINDOWS-1258" },
        { NULL, NULL }
    };

    if (!name)
        return name;

    /* Avoid lots of tests for common correct names. */
    for (cor = correct; *cor; cor++)
        if (!strcmp(name, *cor))
            return name;

    for (ali = aliases; ali->alias; ali++)
        if (!strcmp(name, ali->alias))
            return ali->name;

    for (sub = substrs; sub->substr; sub++)
        if (strstr(name, sub->substr))
            return sub->name;

    return name;
}
