/* lock_file.h -- lock and unlock files.
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

/*
 * Lock open/close routines.
 *
 * The lk[f]opendata() functions are designed to open "data" files (anything
 * not a mail spool file) using the locking mechanism configured for data
 * files.  The lk[f]openspool() functions are for opening the mail spool
 * file, which will use the locking algorithm configured for the mail
 * spool.
 *
 * Files opened for reading are locked with a read lock (if possible by
 * the underlying lock mechanism), files opened for writing are locked
 * using an exclusive lock.  The int * argument is used to indicate failure
 * to acquire a lock.
 */
int lkopendata(const char *file, int access, mode_t mode, int *failed_to_lock);
int lkopenspool(const char *file, int access, mode_t mode, int *failed_to_lock);
FILE *lkfopendata(const char *file, const char *mode, int *failed_to_lock);
FILE *lkfopenspool(const char *file, const char *mode);
int lkclosedata(int fd, const char *name);
int lkfclosedata(FILE *f, const char *name);
int lkclosespool(int fd, const char *name);
int lkfclosespool(FILE *f, const char *name);
