/*
 * m_mktemp.c -- Construct a temporary file.
 *
 * $Id$
 *
 * This code is Copyright (c) 2010, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <errno.h>
#include <h/mh.h>

static char *get_temp_dir();

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
 *    TMP envvar
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
    if (fd_ret != NULL) {
        *fd_ret = fd;
        keep_open = 1;
    }
    if (fp_ret != NULL) {
        FILE *fp = fdopen(fd, "w+");
        if (fp == NULL) {
            int olderr = errno;
            unlink(tmpfil);
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
 * to do string manipulation to generate the desired. pathname prefix.
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
    int n = (int)(cp-dir_in-1); /* Length of dir component */
    snprintf(buffer, sizeof(buffer), "%.*s%s", n, dir_in, pfx_in);
    return m_mktemp(buffer, fd_ret, fp_ret);
}


static char *
get_temp_dir()
{
    // Ignore envvars if we are setuid
    if ((getuid()==geteuid()) && (getgid()==getegid())) {
        char *tmpdir = NULL;
        tmpdir = getenv("MHTMPDIR");
        if (tmpdir != NULL && *tmpdir != '\0') return tmpdir;

        tmpdir = getenv("TMPDIR");
        if (tmpdir != NULL && *tmpdir != '\0') return tmpdir;

        tmpdir = getenv("TMP");
        if (tmpdir != NULL && *tmpdir != '\0') return tmpdir;
    }
    return m_maildir("");
}
