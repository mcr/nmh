
/*
 * m_tmpfil.c -- construct a temporary file
 *
 * $Id$
 */

#include <h/mh.h>


char *
m_tmpfil (char *template)
{
    static char tmpfil[BUFSIZ];

    snprintf (tmpfil, sizeof(tmpfil), "/tmp/%sXXXXXX", template);
/*
  Mkstemp work postponed until later -Doug
#ifdef HAVE_MKSTEMP
    unlink(mkstemp(tmpfil));
#else
*/
    unlink(mktemp(tmpfil));
/*
#endif
*/
    return tmpfil;
}
