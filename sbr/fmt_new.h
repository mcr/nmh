/* fmt_new.h -- read format file/string and normalize
 *
 * This code is Copyright (c) 2017, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information. */

/*
 * Create a new format string.  Arguments are:
 *
 * form		- Name of format file.  Will be searched by etcpath(), see that
 *		  function for details.
 * format	- The format string to be used if no format file is given
 * default_fs	- The default format string to be used if neither form nor
 *		  format is given
 *
 * This function also takes care of processing \ escapes like \n, \t, etc.
 *
 * Returns an allocated format string.
 */
char *new_fs(char *, char *, char *);

/*
 * Free memory allocated by new_fs().  It allocates to a static so
 * no argument is necessary.
 */
void free_fs(void);
