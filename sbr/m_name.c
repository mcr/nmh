
/*
 * m_name.c -- return a message number as a string
 *
 * $Id$
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
