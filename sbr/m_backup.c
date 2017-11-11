/* m_backup.c -- construct a backup file
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "r1bindex.h"
#include "m_backup.h"
#include "m_mktemp.h"


char *
m_backup (const char *file)
{
    const char *cp;
    static char buffer[BUFSIZ];

    if ((cp = r1bindex((char *) file, '/')) == file)
	snprintf(buffer, sizeof(buffer), "%s%s",
		BACKUP_PREFIX, cp);
    else
	snprintf(buffer, sizeof(buffer), "%.*s%s%s", (int)(cp - file), file,
		BACKUP_PREFIX, cp);

    (void) m_unlink(buffer);
    return buffer;
}
