
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
    unlink(mktemp(tmpfil));

    return tmpfil;
}
