
/*
 * mhmail.c -- simple mail program
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/signals.h>
#include <signal.h>

static struct swit switches[] = {
#define	BODYSW             0
    { "body text", 0 },
#define	CCSW               1
    { "cc addrs ...", 0 },
#define	FROMSW             2
    { "from addr", 0 },
#define	SUBJSW             3
    { "subject text", 0 },
#define VERSIONSW          4
    { "version", 0 },
#define	HELPSW             5
    { "help", 0 },
#define	RESNDSW            6
    { "resent", -6 },
#define	QUEUESW            7
    { "queued", -6 },
    { NULL, 0 }
};

static char tmpfil[BUFSIZ];

/*
 * static prototypes
 */
static RETSIGTYPE intrser (int);


int
main (int argc, char **argv)
{
    pid_t child_id;
    int status, i, iscc = 0, nvec;
    int queued = 0, resent = 0, somebody;
    char *cp, *tolist = NULL, *cclist = NULL, *subject = NULL;
    char *from = NULL, *body = NULL, **argp, **arguments;
    char *vec[5], buf[BUFSIZ];
    FILE *out;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* foil search of user profile/context */
    if (context_foil (NULL) == -1)
	done (1);

    /* If no arguments, just incorporate new mail */
    if (argc == 1) {
	execlp (incproc, r1bindex (incproc, '/'), NULL);
	adios (incproc, "unable to exec");
    }

    arguments = getarguments (invo_name, argc, argv, 0);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [addrs ... [switches]]",
			invo_name);
		    print_help (buf, switches, 0);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case FROMSW: 
		    if (!(from = *argp++) || *from == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case BODYSW: 
		    if (!(body = *argp++) || *body == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case CCSW: 
		    iscc++;
		    continue;

		case SUBJSW: 
		    if (!(subject = *argp++) || *subject == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case RESNDSW: 
		    resent++;
		    continue;

		case QUEUESW: 
		    queued++;
		    continue;
	    }
	}
	if (iscc)
	    cclist = cclist ? add (cp, add (", ", cclist)) : getcpy (cp);
	else
	    tolist = tolist ? add (cp, add (", ", tolist)) : getcpy (cp);
    }

    if (tolist == NULL)
	adios (NULL, "usage: %s addrs ... [switches]", invo_name);
    strncpy (tmpfil, m_tmpfil (invo_name), sizeof(tmpfil));
    if ((out = fopen (tmpfil, "w")) == NULL)
	adios (tmpfil, "unable to write");
    chmod (tmpfil, 0600);

    SIGNAL2 (SIGINT, intrser);

    fprintf (out, "%sTo: %s\n", resent ? "Resent-" : "", tolist);
    if (cclist)
	fprintf (out, "%scc: %s\n", resent ? "Resent-" : "", cclist);
    if (subject)
	fprintf (out, "%sSubject: %s\n", resent ? "Resent-" : "", subject);
    if (from)
	fprintf (out, "%sFrom: %s\n", resent ? "Resent-" : "", from);
    if (!resent)
	fputs ("\n", out);

    if (body) {
	fprintf (out, "%s", body);
	if (*body && *(body + strlen (body) - 1) != '\n')
	    fputs ("\n", out);
    } else {
	for (somebody = 0;
		(i = fread (buf, sizeof(*buf), sizeof(buf), stdin)) > 0;
		somebody++)
	    if (fwrite (buf, sizeof(*buf), i, out) != i)
		adios (tmpfil, "error writing");
	if (!somebody) {
	    unlink (tmpfil);
	    done (1);
	}
    }
    fclose (out);

    nvec = 0;
    vec[nvec++] = r1bindex (postproc, '/');
    vec[nvec++] = tmpfil;
    if (resent)
	vec[nvec++] = "-dist";
    if (queued)
	vec[nvec++] = "-queued";
    vec[nvec] = NULL;

    for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	sleep (5);

    if (child_id == NOTOK) {
	/* report failure and then send it */
	admonish (NULL, "unable to fork");
    } else if (child_id) {
	/* parent process */
	if ((status = pidXwait(child_id, postproc))) {
	    fprintf (stderr, "Letter saved in dead.letter\n");
	    execl ("/bin/mv", "mv", tmpfil, "dead.letter", NULL);
	    execl ("/usr/bin/mv", "mv", tmpfil, "dead.letter", NULL);
	    perror ("mv");
	    _exit (-1);
	}
	unlink (tmpfil);
	done (status ? 1 : 0);
    } else {
	/* child process */
	execvp (postproc, vec);
	fprintf (stderr, "unable to exec ");
	perror (postproc);
	_exit (-1);
    }

    return 0;  /* dead code to satisfy the compiler */
}


static RETSIGTYPE
intrser (int i)
{
#ifndef RELIABLE_SIGNALS
    if (i)
	SIGNAL (i, SIG_IGN);
#endif

    unlink (tmpfil);
    done (i != 0 ? 1 : 0);
}

