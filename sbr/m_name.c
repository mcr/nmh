/*
 * m_name.c -- return a message number as a string
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <limits.h>
#include <h/mh.h>

#define STR(s) #s
#define SIZE(n) (sizeof STR(n)) /* Includes NUL. */

char *
m_name (int num)
{
    static char name[SIZE(INT_MAX)];

    if (num <= 0)
	return "?";

    snprintf(name, sizeof name, "%d", num);

    return name;
}
