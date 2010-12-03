
/*
 * whatnow.c -- the nmh `WhatNow' shell
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

/* from whatnowsbr.c */
int WhatNow (int, char **);


int
main (int argc, char **argv)
{
#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    return WhatNow (argc, argv);
}
