
/*
 * m_tmpfil.c -- construct a temporary file
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
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
