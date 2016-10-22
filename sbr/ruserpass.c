/*
 * Portions of this code are
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Portions of this code are Copyright (c) 2013, by the authors of
 * nmh.  See the COPYRIGHT file in the root directory of the nmh
 * distribution for complete copyright information.
 */

#include <h/mh.h>
#include <h/utils.h>
#include <pwd.h>

static FILE *cfile;

#define TOK_EOF 0
#define	DEFAULT	1
#define	LOGIN	2
#define	PASSWD	3
#define	ACCOUNT 4
#define MACDEF  5
#define	ID	10
#define	MACH	11

#define MAX_TOKVAL_SIZE 1024 /* Including terminating NUL. */

struct toktab {
    char *tokstr;
    int tval;
};

static struct toktab toktabs[] = {
    { "",         TOK_EOF },
    { "default",  DEFAULT },
    { "login",    LOGIN },
    { "password", PASSWD },
    { "passwd",   PASSWD },
    { "account",  ACCOUNT },
    { "machine",  MACH },
    { "macdef",   MACDEF },
    { 0,          0 }
};

/*
 * prototypes
 */
static int token(char *);


void
ruserpass(const char *host, char **aname, char **apass, int flags)
{
    int t, usedefault = 0;
    struct stat stb;

    init_credentials_file ();

    cfile = fopen (credentials_file, "r");
    if (cfile == NULL) {
	if (errno != ENOENT)
	    perror (credentials_file);
    } else {
        char tokval[MAX_TOKVAL_SIZE];
        tokval[0] = '\0';

	while ((t = token(tokval))) {
	    switch(t) {
	    case DEFAULT:
		usedefault = 1;
		/* FALL THROUGH */

	    case MACH:
		if (!usedefault) {
		    if (token(tokval) != ID)
			continue;
		    /*
		     * Allow match either for user's host name.
		     */
		    if (strcasecmp(host, tokval) == 0)
			goto match;
		    continue;
		}
	    match:
		while ((t = token(tokval)) && t != MACH && t != DEFAULT) {
		    switch(t) {
		    case LOGIN:
			if (token(tokval) && *aname == 0)
                            *aname = mh_xstrdup(tokval);
			break;

		    case PASSWD:
			if (!credentials_no_perm_check &&
			    fstat(fileno(cfile), &stb) >= 0 &&
			    (stb.st_mode & 077) != 0) {
			    /* We make this a fatal error to force the
			       user to correct it. */
			    advise(NULL, "Error - file %s must not be world or "
				   "group readable.", credentials_file);
			    adios(NULL, "Remove password or correct file "
				  "permissions.");
			}
			if (token(tokval) && *apass == 0)
                            *apass = mh_xstrdup(tokval);
			break;

		    case ACCOUNT:
			break;

		    case MACDEF:
			fclose(cfile);
			return;

		    default:
			fprintf(stderr,
				"Unknown keyword %s in credentials file %s\n",
				tokval, credentials_file);
			break;
		    }
		}
		return;
	    }
	}
    }

    if (!*aname && ! (flags & RUSERPASS_NO_PROMPT_USER)) {
	char tmp[80];
	char *myname;

	if ((myname = getlogin()) == NULL) {
	    struct passwd *pp;

	    if ((pp = getpwuid (getuid())) != NULL)
		myname = pp->pw_name;
	}
	printf("Name (%s:%s): ", host, myname);

	if (fgets(tmp, sizeof(tmp) - 1, stdin) == NULL) {
	    advise ("tmp", "fgets");
	}
        TrimSuffixC(tmp, '\n');
	if (*tmp != '\0' || myname == NULL) {
	    myname = tmp;
	}

        *aname = mh_xstrdup(myname);
    }

    if (!*apass && ! (flags & RUSERPASS_NO_PROMPT_PASSWORD)) {
	char prompt[256];
	char *mypass;

	snprintf(prompt, sizeof(prompt), "Password (%s:%s): ", host, *aname);
	mypass = nmh_getpass(prompt);

	if (*mypass == '\0') {
	    mypass = *aname;
	}

        *aname = mh_xstrdup(mypass);
    }

}

static int
token(char *tokval)
{
    int c;
    const char normalStop[] = "\t\n ,"; /* Each breaks a word. */
    const char *stop;
    char *cp;
    struct toktab *t;

    if (feof(cfile) || ferror(cfile))
	return TOK_EOF;

    stop = normalStop;
    while ((c = getc(cfile)) != EOF && c && strchr(stop, c))
        ;
    if (c == EOF)
	return TOK_EOF;

    cp = tokval;
    if (c == '"')
        /* FIXME: Where is the quoted-string syntax of netrc documented?
         * This code treats «"foo""bar"» as two tokens without further
         * separators. */
        stop = "\"";
    else
	*cp++ = c; /* BUG: If a backslash then it doesn't escape. */

    while ((c = getc(cfile)) != EOF && c && !strchr(stop, c)) {
        if (c == '\\')
            c = getc(cfile); /* BUG: What if this is EOF. */
        *cp++ = c;
        if (cp - tokval > MAX_TOKVAL_SIZE-1) {
            adios(NULL, "credential tokens restricted to length %d",
                  MAX_TOKVAL_SIZE - 1);
        }
    }
    *cp = '\0';

    for (t = toktabs; t->tokstr; t++)
	if (!strcmp(t->tokstr, tokval))
	    return (t->tval);

    return (ID);
}
