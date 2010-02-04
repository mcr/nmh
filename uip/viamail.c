
/*
 * viamail.c -- send multiple files in a MIME message
 *
 * $Id$
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <h/md5.h>
#include <errno.h>
#include <signal.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

static struct swit switches[] = {
#define	TOSW                    0
    { "to mailpath", 0 },
#define	FROMSW                  1
    { "from mailpath", 0 },
#define	SUBJECTSW               2
    { "subject subject", 0 },
#define	PARAMSW                 3
    { "parameters arguments", 0 },
#define	DESCRIPTSW              4
    { "description text", 0 },
#define	COMMENTSW               5
    { "comment text", 0 },
#define	DELAYSW                 6
    { "delay seconds", 0 },
#define	VERBSW                  7
    { "verbose", 0 },
#define	NVERBSW                 8
    { "noverbose", 0 },
#define VERSIONSW               9
    { "version", 0 },
#define	HELPSW                 10
    { "help", 0 },
#define DEBUGSW                11
    { "debug", -5 },
    { NULL, 0 }
};

extern int debugsw;
extern int splitsw;
extern int verbsw;

int ebcdicsw = 0;	/* hack for linking purposes */

/* mhmisc.c */
void set_endian (void);

/* mhoutsbr.c */
int writeBase64aux (FILE *, FILE *);

/*
 * static prototypes
 */
static int via_mail (char *, char *, char *, char *, char *, int, char *);


int
main (int argc, char **argv)
{
    int delay = 0;
    char *f1 = NULL, *f2 = NULL, *f3 = NULL;
    char *f4 = NULL, *f5 = NULL, *f7 = NULL;
    char *cp, buf[BUFSIZ];
    char **argp, **arguments;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* foil search of user profile/context */
    if (context_foil (NULL) == -1)
	done (1);

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
		snprintf (buf, sizeof(buf), "%s [switches]", invo_name);
		print_help (buf, switches, 1);
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);
    
	    case TOSW:
		if (!(f1 = *argp++))
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;
	    case SUBJECTSW:
		if (!(f2 = *argp++))
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;
	    case PARAMSW:
		if (!(f3 = *argp++))
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;
	    case DESCRIPTSW:
		if (!(f4 = *argp++))
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;
	    case COMMENTSW:
		if (!(f5 = *argp++))
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;
	    case DELAYSW:
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument to %s", argp[-2]);

		/*
		 * If there is an error, just reset the delay parameter
		 * to -1.  We will set a default delay later.
		 */
		if (sscanf (cp, "%d", &delay) != 1)
		    delay = -1;
		continue;
	    case FROMSW:
		if (!(f7 = *argp++))
		    adios (NULL, "missing argument to %s", argp[-2]);
		continue;

	    case VERBSW: 
		verbsw = 1;
		continue;
	    case NVERBSW: 
		verbsw = 0;
		continue;

	    case DEBUGSW:
		debugsw = 1;
		continue;
	    }
	}
    }

    set_endian ();

    if (!f1)
	adios (NULL, "missing -viamail \"mailpath\" switch");

    via_mail (f1, f2, f3, f4, f5, delay, f7);
    return 0;  /* dead code to satisfy the compiler */
}


/*
 * VIAMAIL
 */

static int
via_mail (char *mailsw, char *subjsw, char *parmsw, char *descsw,
          char *cmntsw, int delay, char *fromsw)
{
    int	status, vecp = 1;
    char tmpfil[BUFSIZ];
    char *vec[MAXARGS];
    struct stat st;
    FILE *fp;
    char *tfile = NULL;

    umask (~m_gmprot ());

    tfile = m_mktemp2(NULL, invo_name, NULL, &fp);
    if (tfile == NULL) adios("viamail", "unable to create temporary file");
    chmod(tfile, 0600);
    strncpy (tmpfil, tfile, sizeof(tmpfil));

    if (!strchr(mailsw, '@'))
	mailsw = concat (mailsw, "@", LocalName (), NULL);
    fprintf (fp, "To: %s\n", mailsw);

    if (subjsw)
	fprintf (fp, "Subject: %s\n", subjsw);

    if (fromsw) {
	if (!strchr(fromsw, '@'))
	    fromsw = concat (fromsw, "@", LocalName (), NULL);
	fprintf (fp, "From: %s\n", fromsw);
    }

    fprintf (fp, "%s: %s\n", VRSN_FIELD, VRSN_VALUE);
    fprintf (fp, "%s: application/octet-stream", TYPE_FIELD);

    if (parmsw)
	fprintf (fp, "; %s", parmsw);

    if (cmntsw)
	fprintf (fp, "\n\t(%s)", cmntsw);

    if (descsw)
	fprintf (fp, "\n%s: %s", DESCR_FIELD, descsw);

    fprintf (fp, "\n%s: %s\n\n", ENCODING_FIELD, "base64");

    if (fflush (fp))
	adios (tmpfil, "error writing to");

    writeBase64aux (stdin, fp);
    if (fflush (fp))
	adios (tmpfil, "error writing to");

    if (fstat (fileno (fp), &st) == NOTOK)
	adios ("failed", "fstat of %s", tmpfil);

    if (delay < 0)
	splitsw = 10;
    else
	splitsw = delay;

    status = 0;
    vec[0] = r1bindex (postproc, '/');
    if (verbsw)
	vec[vecp++] = "-verbose";

    switch (sendsbr (vec, vecp, tmpfil, &st, 0, (char *)0, 0)) {
	case DONE:
	case NOTOK:
	    status++;
	    break;
	case OK:
	    break;
    }

    fclose (fp);
    if (unlink (tmpfil) == -1)
	advise (NULL, "unable to remove temp file %s", tmpfil);
    done (status);
    return 1;
}
