
/*
 * utils.c -- various utility routines
 *
 * $Id$
 *
 * This code is Copyright (c) 2006, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <stdlib.h>

void *
mh_xmalloc(size_t size)
{
    void *memory;

    if (size == 0)
        adios(NULL, "Tried to malloc 0 bytes");

    memory = malloc(size);
    if (!memory)
        adios(NULL, "Malloc failed");

    return memory;
}

void *
mh_xrealloc(void *ptr, size_t size)
{
    void *memory;

    if (size == 0)
        adios(NULL, "Tried to realloc 0bytes");

    memory = realloc(ptr, size);
    if (!memory)
        adios(NULL, "Realloc failed");

    return memory;
}
char *
pwd(void)
{
    register char *cp;
    static char curwd[PATH_MAX];

    if (!getcwd (curwd, PATH_MAX)) {
        admonish (NULL, "unable to determine working directory");
        if (!mypath || !*mypath
                || (strcpy (curwd, mypath), chdir (curwd)) == -1) {
            strcpy (curwd, "/");
            chdir (curwd);
        }
        return curwd;
    }

    if ((cp = curwd + strlen (curwd) - 1) > curwd && *cp == '/')
        *cp = '\0';

    return curwd;
}
