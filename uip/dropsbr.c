
/*
 * dropsbr.c -- create/read/manipulate mail drops
 *
 * $Id$
 */

#include <h/nmh.h>

#ifndef	MMDFONLY
# include <h/mh.h>
# include <h/dropsbr.h>
# include <zotnet/mts/mts.h>
# include <h/tws.h>
#else
# include "dropsbr.h"
# include "strings.h"
# include "mmdfonly.h"
#endif

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif

#ifdef NTOHLSWAP
# include <netinet/in.h>
#else
# undef ntohl
# define ntohl(n) (n)
#endif

#include <fcntl.h>

extern int errno;

/*
 * static prototypes
 */
static int mbx_chk_mbox (int);
static int mbx_chk_mmdf (int);
static int map_open (char *, int *, int);


/*
 * Main entry point to open/create and lock
 * a file or maildrop.
 */

int
mbx_open (char *file, int mbx_style, uid_t uid, gid_t gid, mode_t mode)
{
    int j, count, fd;
    struct stat st;

    j = 0;

    /* attempt to open and lock file */
    for (count = 4; count > 0; count--) {
	if ((fd = lkopen (file, O_RDWR | O_CREAT | O_NONBLOCK, mode)) == NOTOK) {
	    switch (errno) {
#if defined(FCNTL_LOCKING) || defined(LOCKF_LOCKING)
		case EACCES:
		case EAGAIN:
#endif

#ifdef FLOCK_LOCKING
		case EWOULDBLOCK:
#endif
		case ETXTBSY: 
		    j = errno;
		    sleep (5);
		    break;

		default: 
		    /* just return error */
		    return NOTOK;
	    }
	}

	/* good file descriptor */
	break;
    }

    errno = j;

    /*
     * Return if we still failed after 4 attempts,
     * or we just want to skip the sanity checks.
     */
    if (fd == NOTOK || mbx_style == OTHER_FORMAT)
	return fd;

    /*
     * Do sanity checks on maildrop.
     */
    if (fstat (fd, &st) == NOTOK) {
	/*
	 * The stat failed.  So we make sure file
	 * has right ownership/modes
	 */
	chown (file, uid, gid);
	chmod (file, mode);
    } else if (st.st_size > (off_t) 0) {
	int status;

	/* check the maildrop */
	switch (mbx_style) {
	    case MMDF_FORMAT: 
	    default: 
		status = mbx_chk_mmdf (fd);
		break;

	    case MBOX_FORMAT: 
		status = mbx_chk_mbox (fd);
		break;
	}

	/* if error, attempt to close it */
	if (status == NOTOK) {
	    close (fd);
	    return NOTOK;
	}
    }

    return fd;
}


/*
 * Check/prepare MBOX style maildrop for appending.
 */

static int
mbx_chk_mbox (int fd)
{
    /* just seek to the end */
    if (lseek (fd, (off_t) 0, SEEK_END) == (off_t) NOTOK)
	return NOTOK;

    return OK;
}


/*
 * Check/prepare MMDF style maildrop for appending.
 */

static int
mbx_chk_mmdf (int fd)
{
    size_t count;
    char ldelim[BUFSIZ];

    count = strlen (mmdlm2);

    /* casting -count to off_t, seem to break FreeBSD 2.2.6 */
    if (lseek (fd, (long) (-count), SEEK_END) == (off_t) NOTOK)
	return NOTOK;
    if (read (fd, ldelim, count) != count)
	return NOTOK;

    ldelim[count] = 0;

    if (strcmp (ldelim, mmdlm2)
	    && write (fd, "\n", 1) != 1
	    && write (fd, mmdlm2, count) != count)
	return NOTOK;

    return OK;
}


int
mbx_read (FILE *fp, long pos, struct drop **drops, int noisy)
{
    register int len, size;
    register long ld1, ld2;
    register char *bp;
    char buffer[BUFSIZ];
    register struct drop *cp, *dp, *ep, *pp;

    pp = (struct drop *) calloc ((size_t) (len = MAXFOLDER), sizeof(*dp));
    if (pp == NULL) {
	if (noisy)
	    admonish (NULL, "unable to allocate drop storage");
	return NOTOK;
    }

    ld1 = (long) strlen (mmdlm1);
    ld2 = (long) strlen (mmdlm2);

    fseek (fp, pos, SEEK_SET);
    for (ep = (dp = pp) + len - 1; fgets (buffer, sizeof(buffer), fp);) {
	size = 0;
	if (strcmp (buffer, mmdlm1) == 0)
	    pos += ld1, dp->d_start = (long) pos;
	else {
	    dp->d_start = (long)pos , pos += (long) strlen (buffer);
	    for (bp = buffer; *bp; bp++, size++)
		if (*bp == '\n')
		    size++;
	}

	while (fgets (buffer, sizeof(buffer), fp) != NULL)
	    if (strcmp (buffer, mmdlm2) == 0)
		break;
	    else {
		pos += (long) strlen (buffer);
		for (bp = buffer; *bp; bp++, size++)
		    if (*bp == '\n')
			size++;
	    }

	if (dp->d_start != (long) pos) {
	    dp->d_id = 0;
	    dp->d_size = (long) size;
	    dp->d_stop = pos;
	    dp++;
	}
	pos += ld2;

	if (dp >= ep) {
	    register int    curlen = dp - pp;

	    cp = (struct drop *) realloc ((char *) pp,
		                    (size_t) (len += MAXFOLDER) * sizeof(*pp));
	    if (cp == NULL) {
		if (noisy)
		    admonish (NULL, "unable to allocate drop storage");
		free ((char *) pp);
		return 0;
	    }
	    dp = cp + curlen, ep = (pp = cp) + len - 1;
	}
    }

    if (dp == pp)
	free ((char *) pp);
    else
	*drops = pp;
    return (dp - pp);
}


int
mbx_write(char *mailbox, int md, FILE *fp, int id, long last,
           long pos, off_t stop, int mapping, int noisy)
{
    register int i, j, size;
    off_t start;
    long off;
    register char *cp;
    char buffer[BUFSIZ];

    off = (long) lseek (md, (off_t) 0, SEEK_CUR);
    j = strlen (mmdlm1);
    if (write (md, mmdlm1, j) != j)
	return NOTOK;
    start = lseek (md, (off_t) 0, SEEK_CUR);
    size = 0;

    fseek (fp, pos, SEEK_SET);
    while (fgets (buffer, sizeof(buffer), fp) && (pos < stop)) {
	i = strlen (buffer);
	for (j = 0; (j = stringdex (mmdlm1, buffer)) >= 0; buffer[j]++)
	    continue;
	for (j = 0; (j = stringdex (mmdlm2, buffer)) >= 0; buffer[j]++)
	    continue;
	if (write (md, buffer, i) != i)
	    return NOTOK;
	pos += (long) i;
	if (mapping)
	    for (cp = buffer; i-- > 0; size++)
		if (*cp++ == '\n')
		    size++;
    }

    stop = lseek (md, (off_t) 0, SEEK_CUR);
    j = strlen (mmdlm2);
    if (write (md, mmdlm2, j) != j)
	return NOTOK;
    if (mapping)
	map_write (mailbox, md, id, last, start, stop, off, size, noisy);

    return OK;
}


/*
 * Append message to end of file or maildrop.
 */

int
mbx_copy (char *mailbox, int mbx_style, int md, int fd,
          int mapping, char *text, int noisy)
{
    int i, j, size;
    off_t start, stop;
    long pos;
    char *cp, buffer[BUFSIZ];
    FILE *fp;

    pos = (long) lseek (md, (off_t) 0, SEEK_CUR);
    size = 0;

    switch (mbx_style) {
	case MMDF_FORMAT: 
	default: 
	    j = strlen (mmdlm1);
	    if (write (md, mmdlm1, j) != j)
		return NOTOK;
	    start = lseek (md, (off_t) 0, SEEK_CUR);

	    if (text) {
		i = strlen (text);
		if (write (md, text, i) != i)
		    return NOTOK;
		for (cp = text; *cp++; size++)
		    if (*cp == '\n')
			size++;
	    }
		    
	    while ((i = read (fd, buffer, sizeof(buffer))) > 0) {
		for (j = 0;
			(j = stringdex (mmdlm1, buffer)) >= 0;
			buffer[j]++)
		    continue;
		for (j = 0;
			(j = stringdex (mmdlm2, buffer)) >= 0;
			buffer[j]++)
		    continue;
		if (write (md, buffer, i) != i)
		    return NOTOK;
		if (mapping)
		    for (cp = buffer; i-- > 0; size++)
			if (*cp++ == '\n')
			    size++;
	    }

	    stop = lseek (md, (off_t) 0, SEEK_CUR);
	    j = strlen (mmdlm2);
	    if (write (md, mmdlm2, j) != j)
		return NOTOK;
	    if (mapping)
		map_write (mailbox, md, 0, (long) 0, start, stop, pos, size, noisy);

	    return (i != NOTOK ? OK : NOTOK);

	case MBOX_FORMAT:
	    if ((j = dup (fd)) == NOTOK)
		return NOTOK;
	    if ((fp = fdopen (j, "r")) == NULL) {
		close (j);
		return NOTOK;
	    }
	    start = lseek (md, (off_t) 0, SEEK_CUR);

	    /* If text is given, we add it to top of message */
	    if (text) {
		i = strlen (text);
		if (write (md, text, i) != i)
		    return NOTOK;
		for (cp = text; *cp++; size++)
		    if (*cp == '\n')
			size++;
	    }
		    
	    for (j = 0; fgets (buffer, sizeof(buffer), fp) != NULL; j++) {

		/*
		 * Check the first line, and make some changes.
		 */
		if (j == 0 && !text) {
		    /*
		     * Change the "Return-Path:" field (if in first line)
		     * back to "From ".
		     */
		    if (!strncmp (buffer, "Return-Path:", 12)) {
			char tmpbuffer[BUFSIZ];
			char *tp, *ep, *fp;

			strncpy(tmpbuffer, buffer, sizeof(tmpbuffer));
			ep = tmpbuffer + 13;
			if (!(fp = strchr(ep + 1, ' ')))
			    fp = strchr(ep + 1, '\n');
			tp = dctime(dlocaltimenow());
			snprintf (buffer, sizeof(buffer), "From %.*s  %s",
				fp - ep, ep, tp);
		    } else if (!strncmp (buffer, "X-Envelope-From:", 16)) {
			/*
			 * Change the "X-Envelope-From:" field
			 * (if first line) back to "From ".
			 */
			char tmpbuffer[BUFSIZ];
			char *ep;

			strncpy(tmpbuffer, buffer, sizeof(tmpbuffer));
			ep = tmpbuffer + 17;
			snprintf (buffer, sizeof(buffer), "From %s", ep);
		    } else if (strncmp (buffer, "From ", 5)) {
			/*
			 * If there is already a "From " line,
			 * then leave it alone.  Else we add one.
			 */
			char tmpbuffer[BUFSIZ];
			char *tp, *ep;

			strncpy(tmpbuffer, buffer, sizeof(tmpbuffer));
			ep = "nobody@nowhere";
			tp = dctime(dlocaltimenow());
			snprintf (buffer, sizeof(buffer), "From %s  %s%s", ep, tp, tmpbuffer);
		    }
		}

		/*
		 * If this is not first line, and begins with
		 * "From ", then prepend line with ">".
		 */
		if (j != 0 && strncmp (buffer, "From ", 5) == 0) {
		    write (md, ">", 1);
		    size++;
		}
		i = strlen (buffer);
		if (write (md, buffer, i) != i) {
		    fclose (fp);
		    return NOTOK;
		}
		if (mapping)
		    for (cp = buffer; i-- > 0; size++)
			if (*cp++ == '\n')
			    size++;
	    }
	    if (write (md, "\n", 1) != 1) {
		fclose (fp);
		return NOTOK;
	    }
	    if (mapping)
		size += 2;

	    fclose (fp);
	    lseek (fd, (off_t) 0, SEEK_END);
	    stop = lseek (md, (off_t) 0, SEEK_CUR);
	    if (mapping)
		map_write (mailbox, md, 0, (long) 0, start, stop, pos, size, noisy);

	    return OK;
    }
}


int
mbx_size (int md, off_t start, off_t stop)
{
    register int i, fd;
    register long pos;
    register FILE *fp;

    if ((fd = dup (md)) == NOTOK || (fp = fdopen (fd, "r")) == NULL) {
	if (fd != NOTOK)
	    close (fd);
	return NOTOK;
    }

    fseek (fp, start, SEEK_SET);
    for (i = 0, pos = stop - start; pos-- > 0; i++)
	if (fgetc (fp) == '\n')
	    i++;

    fclose (fp);
    return i;
}


/*
 * Close and unlock file/maildrop.
 */

int
mbx_close (char *mailbox, int md)
{
    lkclose (md, mailbox);
    return OK;
}


/*
 * This function is performed implicitly by getbbent.c:
 *     bb->bb_map = map_name (bb->bb_file);
 */

char *
map_name (char *file)
{
    register char *cp, *dp;
    static char buffer[BUFSIZ];

    if ((dp = strchr(cp = r1bindex (file, '/'), '.')) == NULL)
	dp = cp + strlen (cp);
    if (cp == file)
	snprintf (buffer, sizeof(buffer), ".%.*s%s", dp - cp, cp, ".map");
    else
	snprintf (buffer, sizeof(buffer), "%.*s.%.*s%s",
		cp - file, file, dp - cp, cp, ".map");

    return buffer;
}


int
map_read (char *file, long pos, struct drop **drops, int noisy)
{
    register int i, md, msgp;
    register char *cp;
    struct drop d;
    register struct drop *mp, *dp;

    if ((md = open (cp = map_name (file), O_RDONLY)) == NOTOK
	    || map_chk (cp, md, mp = &d, pos, noisy)) {
	if (md != NOTOK)
	    close (md);
	return 0;
    }

    msgp = mp->d_id;
    dp = (struct drop *) calloc ((size_t) (msgp + 1), sizeof(*dp));
    if (dp == NULL) {
	close (md);
	return 0;
    }

    memcpy((char *) dp, (char *) mp, sizeof(*dp));

    lseek (md, (off_t) sizeof(*mp), SEEK_SET);
    if ((i = read (md, (char *) (dp + 1), msgp * sizeof(*dp))) < sizeof(*dp)) {
	i = 0;
	free ((char *) dp);
    } else {
#ifdef NTOHLSWAP
	register struct drop *tdp;
	int j;

	for (j = 0, tdp = dp; j < i / sizeof(*dp); j++, tdp++) {
	    tdp->d_id = ntohl(tdp->d_id);
	    tdp->d_size = ntohl(tdp->d_size);
	    tdp->d_start = ntohl(tdp->d_start);
	    tdp->d_stop = ntohl(tdp->d_stop);
	}
#endif
	*drops = dp;
    }

    close (md);

    return (i / sizeof(*dp));
}


int
map_write (char *mailbox, int md, int id, long last, off_t start,
           off_t stop, long pos, int size, int noisy)
{
    register int i;
    int clear, fd, td;
    char *file;
    register struct drop *dp;
    struct drop d1, d2, *rp;
    register FILE *fp;

    if ((fd = map_open (file = map_name (mailbox), &clear, md)) == NOTOK)
	return NOTOK;

    if (!clear && map_chk (file, fd, &d1, pos, noisy)) {
	unlink (file);
	mbx_close (file, fd);
	if ((fd = map_open (file, &clear, md)) == NOTOK)
	    return NOTOK;
	clear++;
    }

    if (clear) {
	if ((td = dup (md)) == NOTOK || (fp = fdopen (td, "r")) == NULL) {
	    if (noisy)
		admonish (file, "unable to %s", td != NOTOK ? "fdopen" : "dup");
	    if (td != NOTOK)
		close (td);
	    mbx_close (file, fd);
	    return NOTOK;
	}

	switch (i = mbx_read (fp, 0, &rp, noisy)) {
	    case NOTOK:
		fclose (fp);
		mbx_close (file, fd);
		return NOTOK;

	    case OK:
		break;

	    default:
		d1.d_id = 0;
		for (dp = rp; i-- >0; dp++) {
		    if (dp->d_start == start)
			dp->d_id = id;
		    lseek (fd, (off_t) (++d1.d_id * sizeof(*dp)), SEEK_SET);
		    if (write (fd, (char *) dp, sizeof(*dp)) != sizeof(*dp)) {
			if (noisy)
			    admonish (file, "write error");
			mbx_close (file, fd);
			fclose (fp);
			return NOTOK;
		    }
		}
		free ((char *) rp);
		break;
	}
    }
    else {
	if (last == 0)
	    last = d1.d_start;
	dp = &d2;
	dp->d_id = id;
	dp->d_size = (long) (size ? size : mbx_size (fd, start, stop));
	dp->d_start = start;
	dp->d_stop = stop;
	lseek (fd, (off_t) (++d1.d_id * sizeof(*dp)), SEEK_SET);
	if (write (fd, (char *) dp, sizeof(*dp)) != sizeof(*dp)) {
	    if (noisy)
		admonish (file, "write error");
	    mbx_close (file, fd);
	    return NOTOK;
	}
    }

    dp = &d1;
    dp->d_size = DRVRSN;
    dp->d_start = (long) last;
    dp->d_stop = lseek (md, (off_t) 0, SEEK_CUR);

    lseek (fd, (off_t) 0, SEEK_SET);
    if (write (fd, (char *) dp, sizeof(*dp)) != sizeof(*dp)) {
	if (noisy)
	    admonish (file, "write error");
	mbx_close (file, fd);
	return NOTOK;
    }

    mbx_close (file, fd);

    return OK;
}


static int
map_open (char *file, int *clear, int md)
{
    mode_t mode;
    struct stat st;

    mode = fstat (md, &st) != NOTOK ? (mode_t) (st.st_mode & 0777) : m_gmprot ();
    return mbx_open (file, OTHER_FORMAT, st.st_uid, st.st_gid, mode);
}


int
map_chk (char *file, int fd, struct drop *dp, long pos, int noisy)
{
    long count;
    struct drop d, tmpd;
    register struct drop *dl;

    if (read (fd, (char *) &tmpd, sizeof(*dp)) != sizeof(*dp)) {
#ifdef notdef
	admonish (NULL, "%s: missing or partial index", file);
#endif /* notdef */
	return NOTOK;
    }
#ifndef	NTOHLSWAP
    *dp = tmpd;		/* if ntohl(n)=(n), can use struct assign */
#else
    dp->d_id    = ntohl(tmpd.d_id);
    dp->d_size  = ntohl(tmpd.d_size);
    dp->d_start = ntohl(tmpd.d_start);
    dp->d_stop  = ntohl(tmpd.d_stop);
#endif
    
    if (dp->d_size != DRVRSN) {
	if (noisy)
	    admonish (NULL, "%s: version mismatch (%d != %d)", file,
				dp->d_size, DRVRSN);
	return NOTOK;
    }

    if (dp->d_stop != pos) {
	if (noisy && pos != (long) 0)
	    admonish (NULL,
		    "%s: pointer mismatch or incomplete index (%ld!=%ld)", 
		    file, dp->d_stop, (long) pos);
	return NOTOK;
    }

    if ((long) ((dp->d_id + 1) * sizeof(*dp)) != (long) lseek (fd, (off_t) 0, SEEK_END)) {
	if (noisy)
	    admonish (NULL, "%s: corrupt index(1)", file);
	return NOTOK;
    }

    dl = &d;
    count = (long) strlen (mmdlm2);
    lseek (fd, (off_t) (dp->d_id * sizeof(*dp)), SEEK_SET);
    if (read (fd, (char *) dl, sizeof(*dl)) != sizeof(*dl)
	    || (ntohl(dl->d_stop) != dp->d_stop
		&& ntohl(dl->d_stop) + count != dp->d_stop)) {
	if (noisy)
	    admonish (NULL, "%s: corrupt index(2)", file);
	return NOTOK;
    }

    return OK;
}
