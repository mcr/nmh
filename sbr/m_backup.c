
/*
 * m_backup.c -- construct a backup file
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


char *
m_backup (char *file)
{
    char *cp;
    static char buffer[BUFSIZ];

    if ((cp = r1bindex(file, '/')) == file)
	snprintf(buffer, sizeof(buffer), "%s%s",
		BACKUP_PREFIX, cp);
    else
	snprintf(buffer, sizeof(buffer), "%.*s%s%s", (int)(cp - file), file,
		BACKUP_PREFIX, cp);

    unlink(buffer);
    return buffer;
}
