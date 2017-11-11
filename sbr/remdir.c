/* remdir.c -- remove a directory
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "remdir.h"
#include "context_save.h"
#include "error.h"


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
