/* lock.c -- routines to lock/unlock files
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
#include <h/utils.h>
#include <h/mts.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>
#include <fcntl.h>
#ifdef HAVE_FLOCK
# include <sys/file.h>
#endif

#if defined(HAVE_LIBLOCKFILE)
# include <lockfile.h>
#endif

#ifdef LOCKDIR
char *lockdir = LOCKDIR;
#endif

/* struct for getting name of lock file to create */
struct lockinfo {
    char curlock[BUFSIZ];
#if !defined(HAVE_LIBLOCKFILE)
    char tmplock[BUFSIZ];
#endif
};

/*
 * Number of tries to retry locking
 */
#define LOCK_RETRIES 60

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

enum locktype { FCNTL_LOCKING, FLOCK_LOCKING, LOCKF_LOCKING, DOT_LOCKING };

/* Our saved lock types. */
static enum locktype datalocktype, spoollocktype;

/* top of list containing all open locks */
static struct lock *l_top = NULL;

static int lkopen(const char *, int, mode_t, enum locktype, int *);
static int str2accbits(const char *);

static int lkopen_fcntl (const char *, int, mode_t, int *);
#ifdef HAVE_LOCKF
static int lkopen_lockf (const char *, int, mode_t, int *);
#endif /* HAVE_LOCKF */
#ifdef HAVE_FLOCK
static int lkopen_flock (const char *, int, mode_t, int *);
#endif /* HAVE_FLOCK */

static enum locktype init_locktype(const char *);

static int lkopen_dot (const char *, int, mode_t, int *);
static void lkclose_dot (int, const char *);
static void lockname (const char *, struct lockinfo *, int);
static void timerON (char *, int);
static void timerOFF (int);
static void alrmser (int);

#if !defined(HAVE_LIBLOCKFILE)
static int lockit (struct lockinfo *);
#endif

/*
 * Base functions: determine the data type used to lock files and
 * call the underlying function.
 */

int
lkopendata(const char *file, int access, mode_t mode, int *failed_to_lock)
{
    static bool deja_vu;

    if (!deja_vu) {
        char *dl;

        deja_vu = true;
	if ((dl = context_find("datalocking"))) {
	    datalocktype = init_locktype(dl);
	} else {
	    /* We default to fcntl locking for data files */
	    datalocktype = FCNTL_LOCKING;
	}
    }

    return lkopen(file, access, mode, datalocktype, failed_to_lock);
}


/*
 * Locking using the spool locking algorithm
 */

int lkopenspool(const char *file, int access, mode_t mode, int *failed_to_lock)
{
    static bool deja_vu;

    if (!deja_vu) {
	deja_vu = true;
        spoollocktype = init_locktype(spoollocking);
    }

    return lkopen(file, access, mode, spoollocktype, failed_to_lock);
}


/*
 * Versions of lkopen that return a FILE *
 */

FILE *
lkfopendata(const char *file, const char *mode, int *failed_to_lock)
{
    FILE *fp;
    int oflags = str2accbits(mode);
    int fd;

    if (oflags == -1) {
    	errno = EINVAL;
	return NULL;
    }

    if ((fd = lkopendata(file, oflags, 0666, failed_to_lock)) == -1)
	return NULL;

    if ((fp = fdopen (fd, mode)) == NULL) {
	close (fd);
	return NULL;
    }

    return fp;
}

FILE *
lkfopenspool(const char *file, const char *mode)
{
    FILE *fp;
    int oflags = str2accbits(mode);
    int failed_to_lock = 0;
    int fd;

    if (oflags == -1) {
    	errno = EINVAL;
	return NULL;
    }

    if ((fd = lkopenspool(file, oflags, 0666, &failed_to_lock)) == -1)
	return NULL;

    if ((fp = fdopen (fd, mode)) == NULL) {
	close (fd);
	return NULL;
    }

    return fp;
}


/*
 * Corresponding close functions.
 *
 * A note here: All of the kernel locking functions terminate the lock
 * when the descriptor is closed, so why write the code to explicitly
 * unlock the file?  We only need to do this in the dot-locking case.
 */

int
lkclosedata(int fd, const char *name)
{
    int rc = close(fd);

    if (datalocktype == DOT_LOCKING)
    	lkclose_dot(fd, name);

    return rc;
}

int
lkfclosedata(FILE *f, const char *name)
{
    int fd, rc;

    if (f == NULL)
    	return 0;
    
    fd = fileno(f);
    rc = fclose(f);

    if (datalocktype == DOT_LOCKING)
    	lkclose_dot(fd, name);

    return rc;
}

int
lkclosespool(int fd, const char *name)
{
    int rc = close(fd);

    if (spoollocktype == DOT_LOCKING)
    	lkclose_dot(fd, name);

    return rc;
}

int
lkfclosespool(FILE *f, const char *name)
{
    int fd, rc;

    if (f == NULL)
    	return 0;
    
    fd = fileno(f);
    rc = fclose(f);

    if (spoollocktype == DOT_LOCKING)
    	lkclose_dot(fd, name);

    return rc;
}


/*
 * Convert fopen() mode argument to open() bits
 */

static int
str2accbits(const char *mode)
{
    if (strcmp (mode, "r") == 0)
	return O_RDONLY;
    if (strcmp (mode, "r+") == 0)
	return O_RDWR;
    if (strcmp (mode, "w") == 0)
	return O_WRONLY | O_CREAT | O_TRUNC;
    if (strcmp (mode, "w+") == 0)
	return O_RDWR | O_CREAT | O_TRUNC;
    if (strcmp (mode, "a") == 0)
	return O_WRONLY | O_CREAT | O_APPEND;
    if (strcmp (mode, "a+") == 0)
	return O_RDWR | O_CREAT | O_APPEND;

    errno = EINVAL;
    return -1;
}

/*
 * Internal routine to switch between different locking types.
 */

static int
lkopen (const char *file, int access, mode_t mode, enum locktype ltype,
        int *failed_to_lock)
{
    switch (ltype) {

    case FCNTL_LOCKING:
    	return lkopen_fcntl(file, access, mode, failed_to_lock);

    case DOT_LOCKING:
    	return lkopen_dot(file, access, mode, failed_to_lock);

#ifdef HAVE_FLOCK
    case FLOCK_LOCKING:
    	return lkopen_flock(file, access, mode, failed_to_lock);
#endif /* HAVE_FLOCK */

#ifdef HAVE_LOCKF
    case LOCKF_LOCKING:
    	return lkopen_lockf(file, access, mode, failed_to_lock);
#endif /* HAVE_FLOCK */

    default:
    	adios(NULL, "Internal locking error: unsupported lock type used!");
    }

    return -1;
}


/*
 * Routine to clean up the dot locking file
 */

static void
lkclose_dot (int fd, const char *file)
{
    struct lockinfo lkinfo;

    lockname (file, &lkinfo, 0);	/* get name of lock file */
#if !defined(HAVE_LIBLOCKFILE)
    (void) m_unlink (lkinfo.curlock);	/* remove lock file      */
#else
    lockfile_remove(lkinfo.curlock);
#endif /* HAVE_LIBLOCKFILE */
    timerOFF (fd);			/* turn off lock timer   */
}


/*
 * Open and lock a file, using fcntl locking
 */

static int
lkopen_fcntl(const char *file, int access, mode_t mode, int *failed_to_lock)
{
    int fd, i, saved_errno;
    struct flock flk;

    /*
     * The assumption here is that if you open the file for writing, you
     * need an exclusive lock.
     */

    for (i = 0; i < LOCK_RETRIES; i++) {
    	if ((fd = open(file, access, mode)) == -1)
	    return -1;

	flk.l_start = 0;
	flk.l_len = 0;
	flk.l_type = (access & O_ACCMODE) == O_RDONLY ? F_RDLCK : F_WRLCK;
	flk.l_whence = SEEK_SET;

	if (fcntl(fd, F_SETLK, &flk) != -1)
	    return fd;

	saved_errno = errno;
	close(fd);
	sleep(1);
    }

    *failed_to_lock = 1;
    errno = saved_errno;
    return -1;
}


#ifdef HAVE_FLOCK
/*
 * Open and lock a file, using flock locking
 */

static int
lkopen_flock(const char *file, int access, mode_t mode, int *failed_to_lock)
{
    int fd, i, saved_errno, locktype;

    /*
     * The assumption here is that if you open the file for writing, you
     * need an exclusive lock.
     */

    locktype = (((access & O_ACCMODE) == O_RDONLY) ? LOCK_SH : LOCK_EX) |
    							LOCK_NB;

    for (i = 0; i < LOCK_RETRIES; i++) {
    	if ((fd = open(file, access, mode)) == -1)
	    return -1;

	if (flock(fd, locktype) != -1)
	    return fd;

	saved_errno = errno;
	close(fd);
	sleep(1);
    }

    *failed_to_lock = 1;
    errno = saved_errno;
    return -1;
}
#endif /* HAVE_FLOCK */

/*
 * Open and lock a file, using lockf locking
 */

static int
lkopen_lockf(const char *file, int access, mode_t mode, int *failed_to_lock)
{
    int fd, i, saved_errno, saved_access;

    /*
     * Two notes:
     *
     * Because lockf locks start from the current offset, mask off O_APPEND
     * and seek to the end of the file later if it was requested.
     *
     * lockf locks require write access to the file, so always add it
     * even if it wasn't requested.
     */

    saved_access = access;

    access &= ~O_APPEND;

    if ((access & O_ACCMODE) == O_RDONLY) {
    	access &= ~O_RDONLY;
	access |= O_RDWR;
    }

    for (i = 0; i < LOCK_RETRIES; i++) {
    	if ((fd = open(file, access, mode)) == -1)
	    return -1;

	if (lockf(fd, F_TLOCK, 0) != -1) {
	    /*
	     * Seek to end if requested
	     */
	    if (saved_access & O_APPEND) {
	    	lseek(fd, 0, SEEK_END);
	    }
	    return fd;
	}

	saved_errno = errno;
	close(fd);
	sleep(1);
    }

    *failed_to_lock = 1;
    errno = saved_errno;
    return -1;
}


/*
 * open and lock a file, using dot locking
 */

static int
lkopen_dot (const char *file, int access, mode_t mode, int *failed_to_lock)
{
    int fd;
    struct lockinfo lkinfo;

    /* open the file */
    if ((fd = open (file, access, mode)) == -1)
	return -1;

    /*
     * Get the name of the eventual lock file, as well
     * as a name for a temporary lock file.
     */
    lockname (file, &lkinfo, 1);

#if !defined(HAVE_LIBLOCKFILE)
    {
	int i;
	for (i = 0; i < LOCK_RETRIES; ++i) {
            struct stat st;

	    /* attempt to create lock file */
	    if (lockit (&lkinfo) == 0) {
		/* if successful, turn on timer and return */
		timerON (lkinfo.curlock, fd);
		return fd;
	    }

            /*
             * Abort locking, if we fail to lock after 5 attempts
             * and are never able to stat the lock file.  Or, if
             * we can stat the lockfile but exceed LOCK_RETRIES
             * seconds waiting for it (by falling out of the loop).
             */
            if (stat (lkinfo.curlock, &st) == -1) {
                if (i++ > 5) break;
                sleep (1);
            } else {
                time_t curtime;
                time (&curtime);
                
                /* check for stale lockfile, else sleep */
                if (curtime > st.st_ctime + RSECS)
                    (void) m_unlink (lkinfo.curlock);
                else
                    sleep (1);
            }
            lockname (file, &lkinfo, 1);
	}

        *failed_to_lock = 1;
        return -1;
    }
#else
    if (lockfile_create(lkinfo.curlock, 5, 0) == L_SUCCESS) {
        timerON(lkinfo.curlock, fd);
        return fd;
    }

    close(fd);
    *failed_to_lock = 1;
    return -1;
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
    char *curlock, *tmpfile;

#if 0
    char buffer[128];
#endif

    curlock = li->curlock;

    if ((tmpfile = m_mktemp(li->tmplock, &fd, NULL)) == NULL) {
        inform("unable to create temporary file in %s", li->tmplock);
	return -1;
    }

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
    fd = link(tmpfile, curlock);
    (void) m_unlink(tmpfile);

    return (fd == -1 ? -1 : 0);
}
#endif /* HAVE_LIBLOCKFILE */

/*
 * Get name of lock file, and temporary lock file
 */

static void
lockname (const char *file, struct lockinfo *li, int isnewlock)
{
    int bplen, tmplen;
    char *bp;
    const char *cp;

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
	snprintf (bp, sizeof(li->curlock), "%.*s", (int)(cp - file), file);
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
		     (int)(cp - li->curlock), li->curlock);
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

    NEW(lp);
    lp->l_lock = mh_xstrdup(curlock);
    lp->l_fd = fd;
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

static void
alrmser (int sig)
{
    char *lockfile;
    struct lock *lp;
    NMH_UNUSED (sig);

    /* update the ctime of all the lock files */
    for (lp = l_top; lp; lp = lp->l_next) {
	lockfile = lp->l_lock;
#if !defined(HAVE_LIBLOCKFILE)
	{
	    int j;
	    if (*lockfile && (j = creat (lockfile, 0600)) != -1)
		close (j);
	}
#else
    lockfile_touch(lockfile);
#endif
    }

    /* restart the alarm */
    alarm (NSECS);
}


/*
 * Return a locking algorithm based on the string name
 */

static enum locktype
init_locktype(const char *lockname)
{
    if (strcasecmp(lockname, "fcntl") == 0) {
    	return FCNTL_LOCKING;
    }
    if (strcasecmp(lockname, "lockf") == 0) {
#ifdef HAVE_LOCKF
	return LOCKF_LOCKING;
#else /* ! HAVE_LOCKF */
	adios(NULL, "lockf not supported on this system");
#endif /* HAVE_LOCKF */
    }
    if (strcasecmp(lockname, "flock") == 0) {
#ifdef HAVE_FLOCK
	return FLOCK_LOCKING;
#else /* ! HAVE_FLOCK */
	adios(NULL, "flock not supported on this system");
#endif /* HAVE_FLOCK */
    }
    if (strcasecmp(lockname, "dot") == 0) {
    	return DOT_LOCKING;
    }
    adios(NULL, "Unknown lock type: \"%s\"", lockname);
    /* NOTREACHED */
    return 0;
}
