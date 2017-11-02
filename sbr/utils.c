/* utils.c -- various utility routines
 *
 * This code is Copyright (c) 2006, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "print_help.h"
#include "error.h"
#include "h/utils.h"
#include "h/signals.h"
#include "m_mktemp.h"
#include "makedir.h"
#include <fcntl.h>
#include <limits.h>
#include "read_line.h"

extern char *mhdocdir;

/* plurals gives the letter ess to indicate a plural noun, or an empty
 * string as plurals+1 for the singular noun.  Used by the PLURALS
 * macro. */
const char plurals[] = "s";

/*
 * We allocate space for messages (msgs array)
 * this number of elements at a time.
 */
#define MAXMSGS 256

/* Call malloc(3), exiting on NULL return. */
void *
mh_xmalloc(size_t size)
{
    void *p;

    if (size == 0)
        size = 1; /* Some mallocs don't like 0. */
    p = malloc(size);
    if (!p)
        die("malloc failed, size wanted: %zu", size);

    return p;
}

/* Call realloc(3), exiting on NULL return. */
void *
mh_xrealloc(void *ptr, size_t size)
{
    void *new;

    /* Copy POSIX behaviour, coping with non-POSIX systems. */
    if (size == 0) {
        free(ptr);
        return mh_xmalloc(1); /* Get a unique pointer. */
    }
    if (!ptr)
        return mh_xmalloc(size);

    new = realloc(ptr, size);
    if (!new)
        die("realloc failed, size wanted: %zu", size);

    return new;
}

/* Call calloc(3), exiting on NULL return. */
void *
mh_xcalloc(size_t nelem, size_t elsize)
{
    void *p;

    if (!nelem || !elsize)
        return mh_xmalloc(1); /* Get a unique pointer. */

    p = calloc(nelem, elsize);
    if (!p)
        die("calloc failed, size wanted: %zu * %zu", nelem, elsize);

    return p;
}

/* Duplicate a NUL-terminated string, exit on failure. */
char *
mh_xstrdup(const char *src)
{
    size_t n;
    char *dest;

    n = strlen(src) + 1; /* Ignore possibility of overflow. */
    dest = mh_xmalloc(n);
    memcpy(dest, src, n);

    return dest;
}

/*
 * Return the present working directory, if the current directory does not
 * exist, or is too long, make / the pwd.
 */
char *
pwd(void)
{
    char *cp;
    static char curwd[PATH_MAX];

    if (!getcwd (curwd, PATH_MAX)) {
        inform("unable to determine working directory, continuing...");
        if (!mypath || !*mypath
                || (strcpy (curwd, mypath), chdir (curwd)) == -1) {
            strcpy (curwd, "/");
            if (chdir (curwd) < 0) {
                advise (curwd, "chdir");
            }
        }
        return curwd;
    }

    if ((cp = curwd + strlen (curwd) - 1) > curwd && *cp == '/')
        *cp = '\0';

    return curwd;
}

/* add returns a newly malloc'd string, exiting on failure.  The order
 * of the parameters is unusual.  A NULL parameter is treated as an
 * empty string.  s1 is free'd.  Use mh_xstrdup(s) rather than add(s,
 * NULL), with FENDNULL() if s might be NULL.
 *
 *     add(NULL, NULL) -> ""
 *     add(NULL, "foo") -> "foo"
 *     add("bar", NULL) -> "bar"
 *     add("bar", "foo") -> "foobar"
 */
char *
add (const char *s2, char *s1)
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
addlist (char *list, const char *item)
{
    if (list)
    	list = add (", ", list);

    return add (item, list);
}

/*
 * folder_exists
 *      Check to see if a folder exists.
 */
int
folder_exists(const char *folder)
{
    struct stat st;

    return stat(folder, &st) != -1;
}

/*
 * create_folder
 *      Check to see if a folder exists, if not, prompt the user to create
 *      it.
 */
void
create_folder(char *folder, int autocreate, void (*done_callback)(int))
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
            if (!read_yes_or_no_if_tty (cp))
                done_callback (1);
            free (cp);
        } else if (autocreate == -1) {
            /* do not create, so exit */
            done_callback (1);
        }
        if (!makedir (folder))
            die("unable to create folder %s", folder);
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
        die("oops, num_digits called with negative value");

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
 * Really a simple vector-of-(char *) maintenance routine.
 */
void
app_msgarg(struct msgs_array *msgs, char *cp)
{
	if(msgs->size >= msgs->max) {
		msgs->max += MAXMSGS;
		msgs->msgs = mh_xrealloc(msgs->msgs,
			msgs->max * sizeof(*msgs->msgs));
	}
	msgs->msgs[msgs->size++] = cp;
}

/*
 * Append a message number to an array of them, resizing it if necessary.
 * Like app_msgarg, but with a vector-of-ints instead.
 */

void
app_msgnum(struct msgnum_array *msgs, int msgnum)
{
	if (msgs->size >= msgs->max) {
		msgs->max += MAXMSGS;
		msgs->msgnums = mh_xrealloc(msgs->msgnums,
			msgs->max * sizeof(*msgs->msgnums));
	}
	msgs->msgnums[msgs->size++] = msgnum;
}


/*
 * Finds first occurrence of str in buf.  buf is not a C string but a
 * byte array of length buflen.  str is a null-terminated C string.
 * find_str() does not modify buf but passes back a non-const char *
 * pointer so that the caller can modify it.
 */
char *
find_str (const char buf[], size_t buflen, const char *str)
{
    const size_t len = strlen (str);
    size_t i;

    for (i = 0; i + len <= buflen; ++i, ++buf) {
        if (! memcmp (buf, str, len)) return (char *) buf;
    }

    return NULL;
}


/*
 * Finds last occurrence of str in buf.  buf is not a C string but a
 * byte array of length buflen.  str is a null-terminated C string.
 * find_str() does not modify buf but passes back a non-const char *
 * pointer so that the caller can modify it.
 */
char *
rfind_str (const char buf[], size_t buflen, const char *str)
{
    const size_t len = strlen (str);
    size_t i;

    for (i = 0, buf += buflen - len; i + len <= buflen; ++i, --buf) {
        if (! memcmp (buf, str, len)) return (char *) buf;
    }

    return NULL;
}


/* POSIX doesn't have strcasestr() so emulate it. */
char *
nmh_strcasestr (const char *s1, const char *s2)
{
    const size_t len = strlen (s2);

    if (isupper ((unsigned char) s2[0])  ||  islower ((unsigned char)s2[0])) {
        char first[3];
        first[0] = (char) toupper ((unsigned char) s2[0]);
        first[1] = (char) tolower ((unsigned char) s2[0]);
        first[2] = '\0';

        for (s1 = strpbrk (s1, first); s1; s1 = strpbrk (++s1, first)) {
            if (! strncasecmp (s1, s2, len)) return (char *) s1;
        }
    } else {
        for (s1 = strchr (s1, s2[0]); s1; s1 = strchr (++s1, s2[0])) {
            if (! strncasecmp (s1, s2, len)) return (char *) s1;
        }
    }

    return NULL;
}


/* truncpy copies at most size - 1 chars from non-NULL src to non-NULL,
 * non-overlapping, dst, and ensures dst is NUL terminated.  If size is
 * zero then it aborts as dst cannot be NUL terminated.
 *
 * It's to be used when truncation is intended and correct, e.g.
 * reporting a possibly very long external string back to the user.  One
 * of its advantages over strncpy(3) is it doesn't pad in the common
 * case of no truncation. */
void
trunccpy(char *dst, const char *src, size_t size)
{
    if (!size) {
        inform("trunccpy: zero-length destination: \"%.20s\"",
            src ? src : "null");
        abort();
    }

    if (strnlen(src, size) < size) {
        strcpy(dst, src);
    } else {
        memcpy(dst, src, size - 1);
        dst[size - 1] = '\0';
    }
}


/* has_prefix returns true if non-NULL s starts with non-NULL prefix. */
bool
has_prefix(const char *s, const char *prefix)
{
    while (*s && *s == *prefix) {
        s++;
        prefix++;
    }

    return *prefix == '\0';
}


/* has_suffix returns true if non-NULL s ends with non-NULL suffix. */
bool
has_suffix(const char *s, const char *suffix)
{
    size_t ls, lsuf;

    ls = strlen(s);
    lsuf = strlen(suffix);

    return lsuf <= ls && !strcmp(s + ls - lsuf, suffix);
}


/* has_suffix_c returns true if non-NULL string s ends with a c before the
 * terminating NUL. */
bool
has_suffix_c(const char *s, int c)
{
    return *s && s[strlen(s) - 1] == c;
}


/* trim_suffix_c deletes c from the end of non-NULL string s if it's
 * present, shortening s by 1.  Only one instance of c is removed. */
void
trim_suffix_c(char *s, int c)
{
    if (!*s)
        return;

    s += strlen(s) - 1;
    if (*s == c)
        *s = '\0';
}


/* to_lower runs all of s through tolower(3). */
void
to_lower(char *s)
{
    unsigned char *b;

    for (b = (unsigned char *)s; (*b = tolower(*b)); b++)
        ;
}


/* to_upper runs all of s through toupper(3). */
void
to_upper(char *s)
{
    unsigned char *b;

    for (b = (unsigned char *)s; (*b = toupper(*b)); b++)
        ;
}


int
nmh_init(const char *argv0, bool read_context, bool check_version)
{
    int status = OK;
    char *locale;

    invo_name = r1bindex ((char *) argv0, '/');

    if (setup_signal_handlers()) {
        admonish("sigaction", "unable to set up signal handlers");
    }

    /* POSIX atexit() does not define any error conditions. */
    if (atexit(remove_registered_files_atexit)) {
        admonish("atexit", "unable to register atexit function");
    }

    /* Read context, if supposed to. */
    if (read_context) {
        char *cp;

        context_read();

        bool allow_version_check = true;
        bool check_older_version = false;
        if (!check_version ||
            ((cp = context_find ("Welcome")) && strcasecmp (cp, "disable") == 0)) {
            allow_version_check = false;
        } else if ((cp = getenv ("MHCONTEXT")) != NULL && *cp != '\0') {
            /* Context file comes from $MHCONTEXT, so only print the message
               if the context file has an older version.  If it does, or if it
               doesn't have a version at all, update the version. */
            check_older_version = true;
        }

        /* Check to see if the user is running a different (or older, if
           specified) version of nmh than they had run before, and notify them
           if so. */
        if (allow_version_check  &&  isatty (fileno (stdin))  &&
            isatty (fileno (stdout))  &&  isatty (fileno (stderr))) {
            if (nmh_version_changed (check_older_version)) {
                printf ("==================================================="
                        "=====================\n");
                printf ("Welcome to nmh version %s\n\n", VERSION);
                printf ("See the release notes in %s/NEWS\n\n",
                         mhdocdir);
                print_intro (stdout, 1);
                printf ("\nThis message will not be repeated until "
                        "nmh is next updated.\n");
                printf ("==================================================="
                        "=====================\n\n");

                fputs ("Press enter to continue: ", stdout);
                (void) read_line ();
                putchar ('\n');
            }
        }
    } else {
        if ((status = context_foil(NULL)) != OK) {
            advise("", "failed to create minimal profile/context");
        }
    }

    /* Allow the user to set a locale in their profile.  Otherwise, use the
       "" string to pull it from their environment, see setlocale(3). */
    if ((locale = context_find ("locale")) == NULL) {
        locale = "";
    }

    if (! setlocale (LC_ALL, locale)) {
        inform("setlocale failed, check your LC_ALL, LC_CTYPE, and LANG "
	    "environment variables, continuing...");
    }

    return status;
}


/*
 * Check stored version, and return 1 if out-of-date or non-existent.
 * Because the output of "mhparam version" is prefixed with "nmh-",
 * use that prefix here.
 */
int
nmh_version_changed (int older)
{
    const char *const context_version = context_find("Version");

    if (older) {
        /* Convert the version strings to floats and compare them.  This will
           break for versions with multiple decimal points, etc. */
        const float current_version = strtof (VERSION, NULL);
        const float old_version =
            context_version  &&  has_prefix(context_version, "nmh-")
            ?  strtof (context_version + 4, NULL)
            :  99999999;

        if (context_version == NULL  ||  old_version < current_version) {
            context_replace ("Version", "nmh-" VERSION);
        }

        return old_version < current_version;
    }

    if (context_version == NULL  ||  strcmp(context_version, "nmh-" VERSION) != 0) {
        context_replace ("Version", "nmh-" VERSION);
        return 1;
    }

    return 0;
}


/* contains8bit returns true if any byte from start onwards fails
 * isascii(3), i.e. is outside [0, 0x7f].  If start is NULL it returns
 * false.  Bytes are examined until a NUL byte, or, if end is not NULL,
 * whilst start is before end. */
bool
contains8bit(const char *start, const char *end)
{
    const char *p;
    char c;

    if (!start)
        return false;

    p = start;
    if (end) {
        while (p < end && (c = (*p++)))
            if (!isascii((unsigned char)c))
                return true;
    } else {
        while ((c = (*p++)))
            if (!isascii((unsigned char)c))
                return true;
    }

    return false;
}


/*
 * See if input has any 8-bit bytes.
 */
int
scan_input (int fd, int *eightbit)
{
    int state;
    char buf[BUFSIZ];

    *eightbit = 0;
    lseek(fd, 0, SEEK_SET);

    while ((state = read (fd, buf, sizeof buf)) > 0) {
        if (contains8bit (buf, buf + state)) {
            *eightbit = 1;
            return OK;
        }
    }

    return state == NOTOK  ?  NOTOK  :  OK;
}


/*
 * Convert an int to a char string.
 */
char *
m_str(int value)
{
    return m_strn(value, 0);
}


/*
 * Convert an int to a char string, of limited width if > 0.
 */
#define STR(s) #s
/* SIZE(n) includes NUL.  n must just be digits, not an equation. */
#define SIZE(n) (sizeof STR(n))

char *
m_strn(int value, unsigned int width)
{
    /* Need to include space for negative sign.  But don't use INT_MIN
       because it could be a macro that would fool SIZE(n). */
    static char buffer[SIZE(-INT_MAX)];
    const int num_chars = snprintf(buffer, sizeof buffer, "%d", value);

    return num_chars > 0  &&  (width == 0 || (unsigned int) num_chars <= width)
        ? buffer
        : "?";
}
