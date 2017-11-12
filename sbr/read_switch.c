/* read_switch.c -- prompt the user for an answer from the list
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "read_switch.h"
#include "smatch.h"


int
read_switch (const char *prompt, const struct swit *ansp)
{
    int i;
    char *cp;
    const struct swit *ap;
    char ansbuf[BUFSIZ];

    for (;;) {
	fputs(prompt, stdout);
	fflush (stdout);
	cp = ansbuf;
	while ((i = getchar ()) != '\n') {
	    if (i == EOF)
		return 0;
	    if (cp < &ansbuf[sizeof ansbuf - 1]) {
		i = tolower(i);
		*cp++ = i;
	    }
	}
	*cp = '\0';
	if (ansbuf[0] == '?' || cp == ansbuf) {
	    puts("Options are:");
	    for (ap = ansp; ap->sw; ap++)
		printf ("  %s\n", ap->sw);
	    continue;
	}
	if ((i = smatch (ansbuf, ansp)) < 0) {
	    printf ("%s: %s.\n", ansbuf, i == -1 ? "unknown" : "ambiguous");
	    continue;
	}
	return i;
    }
}
