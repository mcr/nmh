
/*
 * annosbr.c -- prepend annotation to messages
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/tws.h>
#include <fcntl.h>
#include <errno.h>

extern int  errno;

/*
 * static prototypes
 */
static int annosbr (int, char *, char *, char *, int, int);


int
annotate (char *file, char *comp, char *text, int inplace, int datesw)
{
    int i, fd;

    /* open and lock the file to be annotated */
    if ((fd = lkopen (file, O_RDWR, 0)) == NOTOK) {
	switch (errno) {
	    case ENOENT: 
		break;

	    default: 
		admonish (file, "unable to lock and open");
		break;
	}
	return 1;
    }

    i = annosbr (fd, file, comp, text, inplace, datesw);
    lkclose (fd, file);
    return i;
}


static int
annosbr (int fd, char *file, char *comp, char *text, int inplace, int datesw)
{
    int mode, tmpfd;
    char *cp, *sp;
    char buffer[BUFSIZ], tmpfil[BUFSIZ];
    struct stat st;
    FILE *tmp;

    mode = fstat (fd, &st) != NOTOK ? (st.st_mode & 0777) : m_gmprot ();

    strncpy (tmpfil, m_scratch (file, "annotate"), sizeof(tmpfil));

    if ((tmp = fopen (tmpfil, "w")) == NULL) {
	admonish (tmpfil, "unable to create");
	return 1;
    }
    chmod (tmpfil, mode);

    if (datesw)
	fprintf (tmp, "%s: %s\n", comp, dtimenow (0));
    if ((cp = text)) {
	do {
	    while (*cp == ' ' || *cp == '\t')
		cp++;
	    sp = cp;
	    while (*cp && *cp++ != '\n')
		continue;
	    if (cp - sp)
		fprintf (tmp, "%s: %*.*s", comp, cp - sp, cp - sp, sp);
	} while (*cp);
	if (cp[-1] != '\n' && cp != text)
	    putc ('\n', tmp);
    }
    fflush (tmp);
    cpydata (fd, fileno (tmp), file, tmpfil);
    fclose (tmp);

    if (inplace) {
	if ((tmpfd = open (tmpfil, O_RDONLY)) == NOTOK)
	    adios (tmpfil, "unable to open for re-reading");
	lseek (fd, (off_t) 0, SEEK_SET);
	cpydata (tmpfd, fd, tmpfil, file);
	close (tmpfd);
	unlink (tmpfil);
    } else {
	strncpy (buffer, m_backup (file), sizeof(buffer));
	if (rename (file, buffer) == NOTOK) {
	    switch (errno) {
		case ENOENT:	/* unlinked early - no annotations */
		    unlink (tmpfil);
		    break;

		default:
		    admonish (buffer, "unable to rename %s to", file);
		    break;
	    }
	    return 1;
	}
	if (rename (tmpfil, file) == NOTOK) {
	    admonish (file, "unable to rename %s to", tmpfil);
	    return 1;
	}
    }

    return 0;
}
