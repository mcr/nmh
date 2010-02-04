
/*
 * m_scratch.c -- construct a scratch file
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

/***************************************************************************
 * DO NOT USE THIS FUNCTION!  IT WILL BE REMOVED IN THE FUTURE.
 * THIS FUNCTION IS INSECURE.  USE THE FUNCTIONS DEFINED IN m_mktemp.c.
 ***************************************************************************/
char *
m_scratch (char *file, char *template)
{
    char *cp;
    static char buffer[BUFSIZ], tmpfil[BUFSIZ];

    snprintf (tmpfil, sizeof(tmpfil), "%sXXXXXX", template);
/*
  Mkstemp work postponed until later -Doug
#ifdef HAVE_MKSTEMP
    mkstemp (tmpfil);
#else
*/
    mktemp (tmpfil);
/*
#endif
*/
    /* nasty - this really means: if there is no '/' in the path */
    if ((cp = r1bindex (file, '/')) == file)
	strncpy (buffer, tmpfil, sizeof(buffer));
    else
	snprintf (buffer, sizeof(buffer), "%.*s%s", (int)(cp - file), file, tmpfil);
    unlink (buffer);

    return buffer;
}
