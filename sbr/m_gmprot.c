
/*
 * m_gmprot.c -- return the msg-protect value
 *
 * $Id$
 */

#include <h/mh.h>


int
m_gmprot (void)
{
    register char *cp;

    return atooi ((cp = context_find ("msg-protect")) && *cp ? cp : msgprot);
}
