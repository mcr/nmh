/*
 * m_tmpfil.c -- construct a temporary file
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
