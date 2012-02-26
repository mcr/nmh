
/*
 * utils.c -- various utility routines
 *
 * This code is Copyright (c) 2006, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

/*
 * We allocate space for messages (msgs array)
 * this number of elements at a time.
 */
#define MAXMSGS 256

/*
 * Safely call malloc
 */
void *
mh_xmalloc(size_t size)
{
    void *memory;

    if (size == 0)
        adios(NULL, "Tried to malloc 0 bytes");

    memory = malloc(size);
    if (!memory)
        adios(NULL, "Malloc failed");

    return memory;
}

/*
 * Safely call realloc
 */
void *
mh_xrealloc(void *ptr, size_t size)
{
    void *memory;

    /* Some non-POSIX realloc()s don't cope with realloc(NULL,sz) */
    if (!ptr)
        return mh_xmalloc(size);

    if (size == 0)
        adios(NULL, "Tried to realloc 0bytes");

    memory = realloc(ptr, size);
    if (!memory)
        adios(NULL, "Realloc failed");

    return memory;
}

/*
 * Return the present working directory, if the current directory does not
 * exist, or is too long, make / the pwd.
 */
char *
pwd(void)
{
    register char *cp;
    static char curwd[PATH_MAX];

    if (!getcwd (curwd, PATH_MAX)) {
        admonish (NULL, "unable to determine working directory");
        if (!mypath || !*mypath
                || (strcpy (curwd, mypath), chdir (curwd)) == -1) {
            strcpy (curwd, "/");
            chdir (curwd);
        }
        return curwd;
    }

    if ((cp = curwd + strlen (curwd) - 1) > curwd && *cp == '/')
        *cp = '\0';

    return curwd;
}

/*
 * add   -- If "s1" is NULL, this routine just creates a
 *       -- copy of "s2" into newly malloc'ed memory.
 *       --
 *       -- If "s1" is not NULL, then copy the concatenation
 *       -- of "s1" and "s2" (note the order) into newly
 *       -- malloc'ed memory.  Then free "s1".
 */
char *
add (char *s2, char *s1)
{
    char *cp;
    size_t len1 = 0, len2 = 0;

    if (s1)
        len1 = strlen (s1);
    if (s2)
        len2 = strlen (s2);

    cp = mh_xmalloc (len1 + len2 + 1);

    /* Copy s1 and free it */
    if (s1) {
        memcpy (cp, s1, len1);
        free (s1);
    }

    /* Copy s2 */
    if (s2)
        memcpy (cp + len1, s2, len2);

    /* Now NULL terminate the string */
    cp[len1 + len2] = '\0';

    return cp;
}

/*
 * addlist
 *	Append an item to a comma separated list
 */
char *
addlist (char *list, char *item)
{
    if (list)
    	list = add (", ", list);

    return add (item, list);
}

/*
 * folder_exists
 *      Check to see if a folder exists.
 */
int folder_exists(char *folder)
{
    struct stat st;
    int exists = 0;

    if (stat (folder, &st) == -1) {
        /* The folder either doesn't exist, or we hit an error.  Either way
         * return a failure.
         */
        exists = 0;
    } else {
        /* We can see a folder with the right name */
        exists = 1;
    }

    return exists;
}


/*
 * create_folder
 *      Check to see if a folder exists, if not, prompt the user to create
 *      it.
 */
void create_folder(char *folder, int autocreate, void (*done_callback)(int))
{
    struct stat st;
    extern int errno;
    char *cp;

    if (stat (folder, &st) == -1) {
        if (errno != ENOENT)
            adios (folder, "error on folder");
        if (autocreate == 0) {
            /* ask before creating folder */
            cp = concat ("Create folder \"", folder, "\"? ", NULL);
            if (!getanswer (cp))
                done_callback (1);
            free (cp);
        } else if (autocreate == -1) {
            /* do not create, so exit */
            done_callback (1);
        }
        if (!makedir (folder))
            adios (NULL, "unable to create folder %s", folder);
    }
}

/*
 * num_digits
 *      Return the number of digits in a nonnegative integer.
 */
int
num_digits (int n)
{
    int ndigits = 0;

    /* Sanity check */
    if (n < 0)
        adios (NULL, "oops, num_digits called with negative value");

    if (n == 0)
        return 1;

    while (n) {
        n /= 10;
        ndigits++;
    }

    return ndigits;
}

/*
 * Append a message arg to an array of them, resizing it if necessary.
 * The function is written to suit the arg parsing code it was extracted
 * from, and will probably be changed when the other code is cleaned up.
 */
void
app_msgarg(struct msgs_array *msgs, char *cp)
{
	if(msgs->size >= msgs->max)
		msgs->msgs = mh_xrealloc(msgs->msgs, (msgs->max+=MAXMSGS)*sizeof(*msgs->msgs));
	msgs->msgs[msgs->size++] = cp;
}

/* Open a form or components file */
int
open_form(char **form, char *def)
{
	int in;
	if (*form) {
		if ((in = open (etcpath (*form), O_RDONLY)) == NOTOK)
			adios (*form, "unable to open form file");
	} else {
		if ((in = open (etcpath (def), O_RDONLY)) == NOTOK)
			adios (def, "unable to open default components file");
		*form = def;
	}
	return in;
}
