
/*
 * m_gmprot.c -- return the msg-protect value
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


int
m_gmprot (void)
{
    register char *cp;

    return atooi ((cp = context_find ("msg-protect")) && *cp ? cp : msgprot);
}
