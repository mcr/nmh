
/*
 * strerror.c -- get error message string
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

extern int sys_nerr;
extern char *sys_errlist[];


char *
strerror (int errnum)
{
   if (errnum > 0 && errnum < sys_nerr)
	return sys_errlist[errnum];
   else
	return NULL;
}
