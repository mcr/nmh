
/*
 * strerror.c -- get error message string
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
