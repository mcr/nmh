
/*
 * whatnow.c -- the nmh `WhatNow' shell
 *
 * $Id$
 */

#include <h/mh.h>


int
main (int argc, char **argv)
{
#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    return WhatNow (argc, argv);
}
