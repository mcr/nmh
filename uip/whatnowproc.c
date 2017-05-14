/* whatnowproc.c -- exec the "whatnowproc"
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>


/*
 * This routine is used by comp, repl, forw, and dist to exec
 * the "whatnowproc".  It first sets up all the environment
 * variables that the "whatnowproc" will need to check, and
 * then execs the command.  As an optimization, if the
 * "whatnowproc" is the nmh command "whatnow" (typical case),
 * it will call this routine directly without exec'ing it.
 */


int
what_now (char *ed, int nedit, int use, char *file, char *altmsg, int dist,
          struct msgs *mp, char *text, int inplace, char *cwd, int atfile)
{
    int found, k, msgnum, vecp;
    int len, buflen;
    char *bp;
    char buffer[BUFSIZ], *vec[MAXARGS];

    vecp = 0;
    vec[vecp++] = r1bindex (whatnowproc, '/');
    vec[vecp] = NULL;

    setenv("mhdraft", file, 1);
    if (mp)
	setenv("mhfolder", mp->foldpath, 1);
    else
	unputenv ("mhfolder");
    if (altmsg) {
	if (mp == NULL || *altmsg == '/' || cwd == NULL)
	    setenv("mhaltmsg", altmsg, 1);
	else {
	    snprintf (buffer, sizeof(buffer), "%s/%s", mp->foldpath, altmsg);
	    setenv("mhaltmsg", buffer, 1);
	}
    } else {
	unputenv ("mhaltmsg");
    }
    if ((bp = getenv ("mhaltmsg")))/* XXX */
	setenv("editalt", bp, 1);
    snprintf (buffer, sizeof(buffer), "%d", dist);
    setenv("mhdist", buffer, 1);
    if (nedit) {
	unputenv ("mheditor");
    } else {
        if (!ed)
            ed = get_default_editor();
	setenv("mheditor", ed, 1);
    }
    snprintf (buffer, sizeof(buffer), "%d", use);
    setenv("mhuse", buffer, 1);
    snprintf (buffer, sizeof(buffer), "%d", atfile);
    setenv("mhatfile", buffer, 1);

    unputenv ("mhmessages");
    unputenv ("mhannotate");
    unputenv ("mhinplace");

    if (text && mp && !is_readonly(mp)) {
	found = 0;
	bp = buffer;
	buflen = sizeof(buffer);
	for (msgnum = mp->lowmsg; msgnum <= mp->hghmsg; msgnum++) {
	    if (is_selected(mp, msgnum)) {
		snprintf (bp, buflen, "%s%s", found ? " " : "", m_name (msgnum));
		len = strlen (bp);
		bp += len;
		buflen -= len;
		for (k = msgnum + 1; k <= mp->hghmsg && is_selected(mp, k); k++)
		    continue;
		if (--k > msgnum) {
		    snprintf (bp, buflen, "-%s", m_name (k));
		    len = strlen (bp);
		    bp += len;
		    buflen -= len;
		}
		msgnum = k + 1;
		found++;
	    }
	}
	if (found) {
	    setenv("mhmessages", buffer, 1);
	    setenv("mhannotate", text, 1);
	    snprintf (buffer, sizeof(buffer), "%d", inplace);
	    setenv("mhinplace", buffer, 1);
	}
    }

    context_save ();	/* save the context file */
    fflush (stdout);

    if (cwd) {
	if (chdir (cwd) < 0) {
	    advise (cwd, "chdir");
	}
    }

    /*
     * If the "whatnowproc" is the nmh command "whatnow",
     * we run it internally, rather than exec'ing it.
     */
    if (strcmp (vec[0], "whatnow") == 0) {
	WhatNow (vecp, vec);
	done (0);
    }

    execvp (whatnowproc, vec);
    fprintf (stderr, "unable to exec ");
    perror (whatnowproc);

    return 0;
}
