
/*
 * utils.c -- various utility routines
 *
 * $Id$
 *
 * This code is Copyright (c) 2006, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/nmh.h>
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
