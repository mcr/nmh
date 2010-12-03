
/*
 * m_name.c -- return a message number as a string
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>

static char name[BUFSIZ];


char *
m_name (int num)
{
    if (num <= 0)
	return "?";

    snprintf (name, sizeof(name), "%d", num);
    return name;
}
