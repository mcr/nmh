
/*
 * mhl.c -- the nmh message listing program
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

/* prototype from mhlsbr.c */
int mhl (int, char **);


int
main (int argc, char **argv)
{
#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    return done (mhl (argc, argv));
}


/*
 * Cheat: we are loaded with adrparse, which wants a routine called
 * OfficialName().  We call adrparse:getm() with the correct arguments
 * to prevent OfficialName() from being called.  Hence, the following
 * is to keep the loader happy.
 */

char *
OfficialName(char *name)
{
    return name;
}
