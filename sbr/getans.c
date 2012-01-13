
/*
 * getans.c -- get an answer from the user and return a string array
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>
#include <setjmp.h>
#include <signal.h>

static char ansbuf[BUFSIZ];
static jmp_buf sigenv;

/*
 * static prototypes
 */
static void intrser (int);


char **
getans (char *prompt, struct swit *ansp)
{
    int i;
    SIGNAL_HANDLER istat = NULL;
    char *cp, **cpp;

    if (!(setjmp (sigenv))) {
	istat = SIGNAL (SIGINT, intrser);
    } else {
	SIGNAL (SIGINT, istat);
	return NULL;
    }

    for (;;) {
	printf ("%s", prompt);
	fflush (stdout);
	cp = ansbuf;
	while ((i = getchar ()) != '\n') {
	    if (i == EOF)
		longjmp (sigenv, 1);
	    if (cp < &ansbuf[sizeof ansbuf - 1])
		*cp++ = i;
	}
	*cp = '\0';
	if (ansbuf[0] == '?' || cp == ansbuf) {
	    printf ("Options are:\n");
	    print_sw (ALL, ansp, "", stdout);
	    continue;
	}
	cpp = brkstring (ansbuf, " ", NULL);
	switch (smatch (*cpp, ansp)) {
	    case AMBIGSW: 
		ambigsw (*cpp, ansp);
		continue;
	    case UNKWNSW: 
		printf (" -%s unknown. Hit <CR> for help.\n", *cpp);
		continue;
	    default: 
		SIGNAL (SIGINT, istat);
		return cpp;
	}
    }
}


static void
intrser (int i)
{
    NMH_UNUSED (i);

    /*
     * should this be siglongjmp?
     */
    longjmp (sigenv, 1);
}
