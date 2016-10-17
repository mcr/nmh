
/*
 * getfolder.c -- get the current or default folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


char *
getfolder(int wantcurrent)
{
    char *folder;

    /*
     * If wantcurrent == 1, then try the current folder first
     */
    if (wantcurrent && (folder = context_find (pfolder)) && *folder != '\0')
	return folder;

    /*
     * Else try the Inbox profile entry
     */
    if ((folder = context_find (inbox)) && *folder != '\0')
	return folder;

    /*
     * Else return compile time default.
     */
    return defaultfolder;
}
