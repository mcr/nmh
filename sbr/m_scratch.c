
/*
 * m_scratch.c -- construct a scratch file
 *
 * $Id$
 */

#include <h/mh.h>


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
	snprintf (buffer, sizeof(buffer), "%.*s%s", cp - file, file, tmpfil);
    unlink (buffer);

    return buffer;
}
