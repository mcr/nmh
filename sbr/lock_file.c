
/*
 * lock.c -- routines to lock/unlock files
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/* Modified by Ruud de Rooij to support Miquel van Smoorenburg's liblockfile
 *
 * Since liblockfile locking shares most of its code with dot locking, it
 * is enabled by defining both DOT_LOCKING and HAVE_LIBLOCKFILE.
 *
 * Ruud de Rooij <ruud@debian.org>  Sun, 28 Mar 1999 15:34:03 +0200
 */
 
#include <h/mh.h>
#include <h/signals.h>

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef TM_IN_SYS_TIME
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif

#ifdef MMDFONLY
# include <mmdfonly.h>
# include <lockonly.h>
#endif /* MMDFONLY */

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#else
# include <sys/file.h>
#endif

#if defined(LOCKF_LOCKING) || defined(FLOCK_LOCKING)
# include <sys/file.h>
#endif

#include <signal.h>

#if defined(HAVE_LIBLOCKFILE)
#include <lockfile.h>
#endif

#ifdef LOCKDIR
char *lockdir = LOCKDIR;
#endif

/* Are we using any kernel locking? */
#if defined (FLOCK_LOCKING) || defined(LOCKF_LOCKING) || defined(FCNTL_LOCKING)
# define KERNEL_LOCKING
#endif

#ifdef DOT_LOCKING

/* struct for getting name of lock file to create */
struct lockinfo {
    char curlock[BUFSIZ];
#if !defined(HAVE_LIBLOCKFILE)
    char tmplock[BUFSIZ];
#endif
};

/*
 * Amount of time to wait before
 * updating ctime of lock file.
 */
#define	NSECS 20

#if !defined(HAVE_LIBLOCKFILE)
/*
 * How old does a lock file need to be
 * before we remove it.
 */
#define RSECS 180
#endif /* HAVE_LIBLOCKFILE */

/* struct for recording and updating locks */
struct lock {
    int	l_fd;
    char *l_lock;
    struct lock *l_next;
};

/* top of list containing all open locks */
static struct lock *l_top = NULL;
#endif /* DOT_LOCKING */

/*
 * static prototypes
 */
#ifdef KERNEL_LOCKING
static int lkopen_kernel (char *, int, mode_t);
#endif

#ifdef DOT_LOCKING
static int lkopen_dot (char *, int, mode_t);
static int lockit (struct lockinfo *);
static void lockname (char *, struct lockinfo *, int);
static void timerON (char *, int);
static void timerOFF (int);
static RETSIGTYPE alrmser (int);
#endif


/*
 * Base routine to open and lock a file,
 * and return a file descriptor.
 */

int
lkopen (char *file, int access, mode_t mode)
{
#ifdef KERNEL_LOCKING
    return lkopen_kernel(file, access, mode);
#endif

#ifdef DOT_LOCKING
    return lkopen_dot(file, access, mode);
#endif
}


/*
 * Base routine to close and unlock a file,
 * given a file descriptor.
 */

int
lkclose (int fd, char *file)
{
#ifdef FCNTL_LOCKING
    struct flock buf;
#endif

#ifdef DOT_LOCKING
    struct lockinfo lkinfo;
#endif

    if (fd == -1)
	return 0;

#ifdef FCNTL_LOCKING
    buf.l_type   = F_UNLCK;
    buf.l_whence = SEEK_SET;
    buf.l_start  = 0;
    buf.l_len    = 0;
    fcntl(fd, F_SETLK, &buf);
#endif

#ifdef FLOCK_LOCKING
    flock (fd, LOCK_UN);
#endif

#ifdef LOCKF_LOCKING
    /* make sure we unlock the whole thing */
    lseek (fd, (off_t) 0, SEEK_SET);
    lockf (fd, F_ULOCK, 0L);
#endif	

#ifdef DOT_LOCKING
    lockname (file, &lkinfo, 0);	/* get name of lock file */
#if !defined(HAVE_LIBLOCKFILE)
    unlink (lkinfo.curlock);		/* remove lock file      */
#else
    lockfile_remove(lkinfo.curlock);
#endif /* HAVE_LIBLOCKFILE */
    timerOFF (fd);			/* turn off lock timer   */
#endif /* DOT_LOCKING */

    return (close (fd));
}


/*
 * Base routine to open and lock a file,
 * and return a FILE pointer
 */

FILE *
lkfopen (char *file, char *mode)
{
    int fd, access;
    FILE *fp;

    if (strcmp (mode, "r") == 0)
	access = O_RDONLY;
    else if (strcmp (mode, "r+") == 0)
	access = O_RDWR;
    else if (strcmp (mode, "w") == 0)
	access = O_WRONLY | O_CREAT | O_TRUNC;
    else if (strcmp (mode, "w+") == 0)
	access = O_RDWR | O_CREAT | O_TRUNC;
    else if (strcmp (mode, "a") == 0)
	access = O_WRONLY | O_CREAT | O_APPEND;
    else if (strcmp (mode, "a+") == 0)
	access = O_RDWR | O_CREAT | O_APPEND;
    else {
	errno = EINVAL;
	return NULL;
    }

    if ((fd = lkopen (file, access, 0666)) == -1)
	return NULL;

    if ((fp = fdopen (fd, mode)) == NULL) {
	close (fd);
	return NULL;
    }

    return fp;
}


/*
 * Base routine to close and unlock a file,
 * given a FILE pointer
 */

int
lkfclose (FILE *fp, char *file)
{
#ifdef FCNTL_LOCKING
    struct flock buf;
#endif

#ifdef DOT_LOCKING
    struct lockinfo lkinfo;
#endif

    if (fp == NULL)
	return 0;

#ifdef FCNTL_LOCKING
    buf.l_type   = F_UNLCK;
    buf.l_whence = SEEK_SET;
    buf.l_start  = 0;
    buf.l_len    = 0;
    fcntl(fileno(fp), F_SETLK, &buf);
#endif

#ifdef FLOCK_LOCKING
    flock (fileno(fp), LOCK_UN);
#endif

#ifdef LOCKF_LOCKING
    /* make sure we unlock the whole thing */
    fseek (fp, 0L, SEEK_SET);
    lockf (fileno(fp), F_ULOCK, 0L);
#endif

#ifdef DOT_LOCKING
    lockname (file, &lkinfo, 0);	/* get name of lock file */
#if !defined(HAVE_LIBLOCKFILE)
    unlink (lkinfo.curlock);		/* remove lock file      */
#else
    lockfile_remove(lkinfo.curlock);
#endif /* HAVE_LIBLOCKFILE */
    timerOFF (fileno(fp));		/* turn off lock timer   */
#endif /* DOT_LOCKING */

    return (fclose (fp));
}


#ifdef KERNEL_LOCKING

/*
 * open and lock a file, using kernel locking
 */

static int
lkopen_kernel (char *file, int access, mode_t mode)
{
    int fd, i, j;

# ifdef FCNTL_LOCKING
    struct flock buf;
# endif /* FCNTL_LOCKING */

    for (i = 0; i < 5; i++) {

# if defined(LOCKF_LOCKING) || defined(FCNTL_LOCKING)
	/* remember the original mode */
	j = access;

	/* make sure we open at the beginning */
	access &= ~O_APPEND;

	/*
	 * We MUST have write permission or
	 * lockf/fcntl() won't work
	 */
	if ((access & 03) == O_RDONLY) {
	    access &= ~O_RDONLY;
	    access |= O_RDWR;
	}
# endif /* LOCKF_LOCKING || FCNTL_LOCKING */

	if ((fd = open (file, access | O_NDELAY, mode)) == -1)
	    return -1;

# ifdef FCNTL_LOCKING
	buf.l_type   = F_WRLCK;
	buf.l_whence = SEEK_SET;
	buf.l_start  = 0;
	buf.l_len    = 0;
	if (fcntl (fd, F_SETLK, &buf) != -1)
	    return fd;
# endif

# ifdef FLOCK_LOCKING
	if (flock (fd, (((access & 03) == O_RDONLY) ? LOCK_SH : LOCK_EX)
		   | LOCK_NB) != -1)
	    return fd;
# endif

# ifdef LOCKF_LOCKING
	if (lockf (fd, F_TLOCK, 0L) != -1) {
	    /* see if we should be at the end */
	    if (j & O_APPEND)
		lseek (fd, (off_t) 0, SEEK_END);
	    return fd;
	}
# endif

	j = errno;
	close (fd);
	sleep (5);
    }

    close (fd);
    errno = j;
    return -1;
}

#endif /* KERNEL_LOCKING */


#ifdef DOT_LOCKING

/*
 * open and lock a file, using dot locking
 */

static int
lkopen_dot (char *file, int access, mode_t mode)
{
    int i, fd;
    time_t curtime;
    struct lockinfo lkinfo;
    struct stat st;

    /* open the file */
    if ((fd = open (file, access, mode)) == -1)
	return -1;

    /*
     * Get the name of the eventual lock file, as well
     * as a name for a temporary lock file.
     */
    lockname (file, &lkinfo, 1);

#if !defined(HAVE_LIBLOCKFILE)
    for (i = 0;;) {
	/* attempt to create lock file */
	if (lockit (&lkinfo) == 0) {
	    /* if successful, turn on timer and return */
	    timerON (lkinfo.curlock, fd);
	    return fd;
	} else {
	    /*
	     * Abort locking, if we fail to lock after 5 attempts
	     * and are never able to stat the lock file.
	     */
	    if (stat (lkinfo.curlock, &st) == -1) {
		if (i++ > 5)
		    return -1;
		sleep (5);
	    } else {
		i = 0;
		time (&curtime);

		/* check for stale lockfile, else sleep */
		if (curtime > st.st_ctime + RSECS)
		    unlink (lkinfo.curlock);
		else
		    sleep (5);
	    }
	}
    }
#else
    if (lockfile_create(lkinfo.curlock, 5, 0) == L_SUCCESS) {
        timerON(lkinfo.curlock, fd);
        return fd;
    }
    else {
        close(fd);
        return -1;
    }
#endif /* HAVE_LIBLOCKFILE */
}

#if !defined(HAVE_LIBLOCKFILE)
/*
 * Routine that actually tries to create
 * the lock file.
 */

static int
lockit (struct lockinfo *li)
{
    int fd;
    char *curlock, *tmplock;

#if 0
    char buffer[128];
#endif

    curlock = li->curlock;
    tmplock = li->tmplock;

#ifdef HAVE_MKSTEMP
    if ((fd = mkstemp(tmplock)) == -1)
	return -1;
#else
    if (mktemp(tmplock) == NULL)
	return -1;
    if (unlink(tmplock) == -1 && errno != ENOENT)
	return -1;
    /* create the temporary lock file */
    if ((fd = creat(tmplock, 0600)) == -1)
	return -1;
#endif

#if 0
    /* write our process id into lock file */
    snprintf (buffer, sizeof(buffer), "nmh lock: pid %d\n", (int) getpid());
    write(fd, buffer, strlen(buffer) + 1);
#endif

    close (fd);

    /*
     * Now try to create the real lock file
     * by linking to the temporary file.
     */
    fd = link(tmplock, curlock);
    unlink(tmplock);

    return (fd == -1 ? -1 : 0);
}
#endif /* HAVE_LIBLOCKFILE */

/*
 * Get name of lock file, and temporary lock file
 */

static void
lockname (char *file, struct lockinfo *li, int isnewlock)
{
    int bplen, tmplen;
    char *bp, *cp;

#if 0
    struct stat st;
#endif

    if ((cp = strrchr (file, '/')) == NULL || *++cp == 0)
	cp = file;

    bp = li->curlock;
    bplen = 0;
#ifdef LOCKDIR
    snprintf (bp, sizeof(li->curlock), "%s/", lockdir);
    tmplen = strlen (bp);
    bp    += tmplen;
    bplen += tmplen;
#else
    if (cp != file) {
	snprintf (bp, sizeof(li->curlock), "%.*s", cp - file, file);
	tmplen = strlen (bp);
	bp    += tmplen;
	bplen += tmplen;
    }
#endif

#if 0
    /*
     * mmdf style dot locking.  Currently not supported.
     * If we start supporting mmdf style dot locking,
     * we will need to change the return value of lockname
     */
    if (stat (file, &st) == -1)
	return -1;

    snprintf (bp, sizeof(li->curlock) - bplen, "LCK%05d.%05d",
	st.st_dev, st.st_ino);
#endif

    snprintf (bp, sizeof(li->curlock) - bplen, "%s.lock", cp);

#if !defined(HAVE_LIBLOCKFILE)
    /*
     * If this is for a new lock, create a name for
     * the temporary lock file for lockit()
     */
    if (isnewlock) {
	if ((cp = strrchr (li->curlock, '/')) == NULL || *++cp == 0)
	    strncpy (li->tmplock, ",LCK.XXXXXX", sizeof(li->tmplock));
	else
	    snprintf (li->tmplock, sizeof(li->tmplock), "%.*s,LCK.XXXXXX",
		     cp - li->curlock, li->curlock);
    }
#endif
}


/*
 * Add new lockfile to the list of open lockfiles
 * and start the lock file timer.
 */

static void
timerON (char *curlock, int fd)
{
    struct lock *lp;
    size_t len;

    if (!(lp = (struct lock *) malloc (sizeof(*lp))))
	return;

    len = strlen(curlock) + 1;
    lp->l_fd = fd;
    if (!(lp->l_lock = malloc (len))) {
	free ((char *) lp);
	return;
    }
    memcpy (lp->l_lock, curlock, len);
    lp->l_next = l_top;

    if (!l_top) {
	/* perhaps SIGT{STP,TIN,TOU} ? */
	SIGNAL (SIGALRM, alrmser);
	alarm (NSECS);
    }

    l_top = lp;
}


/*
 * Search through the list of lockfiles for the
 * current lockfile, and remove it from the list.
 */

static void
timerOFF (int fd)
{
    struct lock *pp, *lp;

    alarm(0);

    if (l_top) {
	for (pp = lp = l_top; lp; pp = lp, lp = lp->l_next) {
	    if (lp->l_fd == fd)
		break;
	}
	if (lp) {
	    if (lp == l_top)
		l_top = lp->l_next;
	    else
		pp->l_next = lp->l_next;

	    free (lp->l_lock);
	    free (lp);
	}
    }

    /* if there are locks left, restart timer */
    if (l_top)
	alarm (NSECS);
}


/*
 * If timer goes off, we update the ctime of all open
 * lockfiles, so another command doesn't remove them.
 */

static RETSIGTYPE
alrmser (int sig)
{
    int j;
    char *lockfile;
    struct lock *lp;

#ifndef	RELIABLE_SIGNALS
    SIGNAL (SIGALRM, alrmser);
#endif

    /* update the ctime of all the lock files */
    for (lp = l_top; lp; lp = lp->l_next) {
	lockfile = lp->l_lock;
#if !defined(HAVE_LIBLOCKFILE)
	if (*lockfile && (j = creat (lockfile, 0600)) != -1)
	    close (j);
#else
    lockfile_touch(lockfile);
#endif
    }

    /* restart the alarm */
    alarm (NSECS);
}

#endif /* DOT_LOCKING */
