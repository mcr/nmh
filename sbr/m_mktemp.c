/*
 * m_mktemp.c -- Construct a temporary file.
 *
 * This code is Copyright (c) 2010, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <h/signals.h>

static void register_for_removal(const char *);


/*  Create a temporary file.  If pfx_in is null, the temporary file
 *  will be created in the temporary directory (more on that later).
 *  If pfx_in is not null, then the temporary file location will be
 *  defined by the value pfx_in.
 *
 *  The file created will be at the pathname specified appended with
 *  6 random (we hope :) characters.
 *
 *  The return value will be the pathname to the file created.
 *
 *  CAUTION: The return pointer references static data.  If
 *  you need to modify, or save, the return string, make a copy of it
 *  first.
 *
 *  When pfx_in is null, the temporary directory is determined as
 *  follows, in order:
 *
 *    MHTMPDIR envvar
 *    TMPDIR envvar
 *    User's mail directory.
 *
 *  NOTE: One will probably use m_mktemp2() instead of this function.
 *  For example, if you want to create a temp file in the defined
 *  temporary directory, but with a custom basename prefix, do
 *  something like the following:
 *
 *    char *tmp_pathname = m_mktemp2(NULL, "mypre", ...);
 */
char *
m_mktemp (
    const char *pfx_in,   /* Pathname prefix for temporary file. */
    int *fd_ret,          /* (return,optional) File descriptor to temp file. */
    FILE **fp_ret         /* (return,optional) FILE pointer to temp file. */
)
{
    static char tmpfil[BUFSIZ];
    int fd = -1;
    int keep_open = 0;
    mode_t oldmode = umask(077);

    if (pfx_in == NULL) {
        snprintf(tmpfil, sizeof(tmpfil), "%s/nmhXXXXXX", get_temp_dir());
    } else {
        snprintf(tmpfil, sizeof(tmpfil), "%sXXXXXX", pfx_in);
    }

    fd = mkstemp(tmpfil);

    if (fd < 0) {
        umask(oldmode);
        return NULL;
    }

    register_for_removal(tmpfil);

    if (fd_ret != NULL) {
        *fd_ret = fd;
        keep_open = 1;
    }
    if (fp_ret != NULL) {
        FILE *fp = fdopen(fd, "w+");
        if (fp == NULL) {
            int olderr = errno;
            (void) m_unlink(tmpfil);
            close(fd);
            errno = olderr;
            umask(oldmode);
            return NULL;
        }
        *fp_ret = fp;
        keep_open = 1;
    }
    if (!keep_open) {
        close(fd);
    }
    umask(oldmode);
    return tmpfil;
}

/* This version allows one to specify the directory the temp file should
 * by created based on a given pathname.  Although m_mktemp() technically
 * supports this, this version is when the directory is defined by
 * a separate variable from the prefix, eliminating the caller from having
 * to do string manipulation to generate the desired pathname prefix.
 *
 * The pfx_in parameter specifies a basename prefix for the file.  If dir_in
 * is NULL, then the defined temporary directory (see comments to m_mktemp()
 * above) is used to create the temp file.
 */
char *
m_mktemp2 (
    const char *dir_in,   /* Directory to place temp file. */
    const char *pfx_in,   /* Basename prefix for temp file. */
    int *fd_ret,          /* (return,optional) File descriptor to temp file. */
    FILE **fp_ret         /* (return,optional) FILE pointer to temp file. */
)
{
    static char buffer[BUFSIZ];
    char *cp;
    int n;

    if (dir_in == NULL) {
        if (pfx_in == NULL) {
            return m_mktemp(NULL, fd_ret, fp_ret);
        }
        snprintf(buffer, sizeof(buffer), "%s/%s", get_temp_dir(), pfx_in);
        return m_mktemp(buffer, fd_ret, fp_ret);
    }

    if ((cp = r1bindex ((char *)dir_in, '/')) == dir_in) {
        /* No directory component */
        return m_mktemp(pfx_in, fd_ret, fp_ret);
    }
    n = (int)(cp-dir_in); /* Length of dir component */
    snprintf(buffer, sizeof(buffer), "%.*s%s", n, dir_in, pfx_in);
    return m_mktemp(buffer, fd_ret, fp_ret);
}


char *
get_temp_dir()
{
    /* Ignore envvars if we are setuid */
    if ((getuid()==geteuid()) && (getgid()==getegid())) {
        char *tmpdir = NULL;
        tmpdir = getenv("MHTMPDIR");
        if (tmpdir != NULL && *tmpdir != '\0') return tmpdir;

        tmpdir = getenv("TMPDIR");
        if (tmpdir != NULL && *tmpdir != '\0') return tmpdir;
    }
    return m_maildir("");
}


/*
 * Array of files (full pathnames) to remove on process exit.
 * Instead of removing slots from the array, just set to NULL
 * to indicate that the file should no longer be removed.
 */
static svector_t exit_filelist = NULL;

/*
 * Register a file for removal at program termination.
 */
static void
register_for_removal(const char *pathname) {
    if (exit_filelist == NULL) exit_filelist = svector_create(20);
    (void) svector_push_back(exit_filelist, add(pathname, NULL));
}

/*
 * Unregister all files that were registered to be removed at program
 * termination.  When called with remove_files of 0, this function is
 * intended for use by one of the programs after a fork(3) without a
 * subsequent call to one of the exec(3) functions or _exit(3).  When
 * called with non-0 remove_files argument, it is intended for use by
 * an atexit() function.
 *
 * Right after a fork() and before calling exec() or _exit(), if the
 * child catches one of the appropriate signals, it will remove
 * all the registered temporary files.  Some of those may be needed by
 * the parent.  Some nmh programs ignore or block some of the signals
 * in the child right after fork().  For the other programs, rather
 * than complicate things by preventing that, just take the chance
 * that it might happen.  It is harmless to attempt to unlink a
 * temporary file twice, assuming another one wasn't created too
 * quickly created with the same name.
 */
void
unregister_for_removal(int remove_files) {
    if (exit_filelist) {
        size_t i;

        for (i = 0; i < svector_size(exit_filelist); ++i) {
            char *filename = svector_at(exit_filelist, i);

            if (filename) {
                if (remove_files) (void) unlink(filename);
                free(filename);
            }
        }

        svector_free(exit_filelist);
        exit_filelist = NULL;
    }
}

/*
 * If the file was registered for removal, deregister it.  In
 * any case, unlink it.
 */
int
m_unlink(const char *pathname) {
    if (exit_filelist) {
        char **slot = svector_find(exit_filelist, pathname);

        if (slot  &&  *slot) {
            free(*slot);
            *slot = NULL;
        }
    }

    return unlink(pathname);
    /* errno should be set to ENOENT if file was not found */
}

/*
 * Remove all registered temporary files.
 */
void
remove_registered_files_atexit() {
    unregister_for_removal(1);
}

/*
 * Remove all registered temporary files.  Suitable for use as a
 * signal handler.  If the signal is one of the usual process
 * termination signals, calls exit().  Otherwise, disables itself
 * by restoring the default action and then re-raises the signal,
 * in case the use was expecting a core dump.
 */
void
remove_registered_files(int sig) {
    struct sigaction act;

    /*
     * Ignore this signal for the duration.  And we either exit() or
     * disable this signal handler below, so don't restore this handler.
     */
    act.sa_handler = SIG_IGN;
    (void) sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    (void) sigaction(sig, &act, 0);

    if (sig == SIGHUP || sig == SIGINT || sig == SIGQUIT || sig == SIGTERM) {
        /*
         * We don't need to call remove_registered_files_atexit() if
         * it was registered with atexit(), but let's not assume that.
         * It's harmless to call it twice.
         */
        remove_registered_files_atexit();

        exit(1);
    } else {
        act.sa_handler = SIG_DFL;
        (void) sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        (void) sigaction(sig, &act, 0);

        remove_registered_files_atexit();

        (void) raise(sig);
    }
}
