
/*
 * fmt_new.c -- read format file/string and normalize
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

#define QUOTE '\\'

static char *formats = 0;

/*
 * static prototypes
 */
static void normalize (char *);


/*
 * Get new format string
 */

char *
new_fs (char *form, char *format, char *default_fs)
{
    struct stat st;
    register FILE *fp;

    if (formats)
	free (formats);

    if (form) {
	if ((fp = fopen (etcpath (form), "r")) == NULL)
	    adios (form, "unable to open format file");

	if (fstat (fileno (fp), &st) == -1)
	    adios (form, "unable to stat format file");

	if (!(formats = malloc ((size_t) st.st_size + 1)))
	    adios (form, "unable to allocate space for format");

	if (read (fileno(fp), formats, (int) st.st_size) != st.st_size)
	    adios (form, "error reading format file");

	formats[st.st_size] = '\0';

	fclose (fp);
    } else {
	formats = getcpy (format ? format : default_fs);
    }

    normalize (formats);	/* expand escapes */

    return formats;
}


/*
 * Expand escapes in format strings
 */

static void
normalize (char *cp)
{
    char *dp;

    for (dp = cp; *cp; cp++) {
	if (*cp != QUOTE) {
	    *dp++ = *cp;
	} else {
	    switch (*++cp) {
		case 'b':
		    *dp++ = '\b';
		    break;

		case 'f':
		    *dp++ = '\f';
		    break;

		case 'n':
		    *dp++ = '\n';
		    break;

		case 'r':
		    *dp++ = '\r';
		    break;

		case 't':
		    *dp++ = '\t';
		    break;

		case '\n':
		    break;

		case 0: 
		    cp--;	/* fall */
		default: 
		    *dp++ = *cp;
		    break;
	    }
	}
    }
    *dp = '\0';
}
