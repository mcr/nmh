
/*
 * remdir.c -- remove a directory
 *
 * $Id$
 */

#include <h/mh.h>


int
remdir (char *dir)
{
    context_save();	/* save the context file */
    fflush(stdout);

    if (rmdir(dir) == -1) {
	admonish (dir, "unable to remove directory");
	return 0;
    }
    return 1;
}
