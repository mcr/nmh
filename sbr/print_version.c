
/*
 * print_version.c -- print a version string
 *
 * $Id$
 */

#include <h/mh.h>


void
print_version (char *invo_name)
{
    printf("%s -- %s\n", invo_name, version_str);
}
