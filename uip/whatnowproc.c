
/*
 * whatnowproc.c -- exec the "whatnowproc"
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
    register char *bp;
    char buffer[BUFSIZ], *vec[MAXARGS];

    vecp = 0;
    vec[vecp++] = r1bindex (whatnowproc, '/');
    vec[vecp] = NULL;

    m_putenv ("mhdraft", file);
    if (mp)
	m_putenv ("mhfolder", mp->foldpath);
    else
	unputenv ("mhfolder");
    if (altmsg) {
	if (mp == NULL || *altmsg == '/' || cwd == NULL)
	    m_putenv ("mhaltmsg", altmsg);
	else {
	    snprintf (buffer, sizeof(buffer), "%s/%s", mp->foldpath, altmsg);
	    m_putenv ("mhaltmsg", buffer);
	}
    } else {
	unputenv ("mhaltmsg");
    }
    if ((bp = getenv ("mhaltmsg")))/* XXX */
	m_putenv ("editalt", bp);
    snprintf (buffer, sizeof(buffer), "%d", dist);
    m_putenv ("mhdist", buffer);
    if (nedit) {
	unputenv ("mheditor");
    } else {
	m_putenv ("mheditor", ed ? ed : (ed = get_default_editor()));
    }
    snprintf (buffer, sizeof(buffer), "%d", use);
    m_putenv ("mhuse", buffer);
    snprintf (buffer, sizeof(buffer), "%d", atfile);
    m_putenv ("mhatfile", buffer);

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
	    m_putenv ("mhmessages", buffer);
	    m_putenv ("mhannotate", text);
	    snprintf (buffer, sizeof(buffer), "%d", inplace);
	    m_putenv ("mhinplace", buffer);
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
