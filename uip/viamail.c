
/*
 * viamail.c -- send multiple files in a MIME message
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/md5.h>
#include <h/mts.h>
#include <h/tws.h>
#include <h/mime.h>
#include <h/mhparse.h>

#define VIAMAIL_SWITCHES \
    X("to mailpath", 0, TOSW) \
    X("from mailpath", 0, FROMSW) \
    X("subject subject", 0, SUBJECTSW) \
    X("parameters arguments", 0, PARAMSW) \
    X("description text", 0, DESCRIPTSW) \
    X("comment text", 0, COMMENTSW) \
    X("delay seconds", 0, DELAYSW) \
    X("verbose", 0, VERBSW) \
    X("noverbose", 0, NVERBSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \
    X("debug", -5, DEBUGSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(VIAMAIL);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(VIAMAIL, switches);
#undef X

extern int debugsw;
extern int splitsw;
extern int verbsw;

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

    if (nmh_init(argv[0], 1)) { return 1; }

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
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);
    
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
    int	status, vecp;
    char tmpfil[BUFSIZ], *program;
    char **vec;
    struct stat st;
    FILE *fp;
    char *tfile = NULL;
    char *cp;

    umask (~m_gmprot ());

    if ((tfile = m_mktemp2(NULL, invo_name, NULL, &fp)) == NULL) {
	adios(NULL, "unable to create temporary file in %s", get_temp_dir());
    }
    strncpy (tmpfil, tfile, sizeof(tmpfil));

    if (!strchr(mailsw, '@'))
	mailsw = concat (mailsw, "@", LocalName (0), NULL);
    fprintf (fp, "To: %s\n", mailsw);

    if (subjsw)
	fprintf (fp, "Subject: %s\n", subjsw);

    if (fromsw) {
	if (!strchr(fromsw, '@'))
	    fromsw = concat (fromsw, "@", LocalName (0), NULL);
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

    writeBase64aux (stdin, fp, 0);
    if (fflush (fp))
	adios (tmpfil, "error writing to");

    if (fstat (fileno (fp), &st) == NOTOK)
	adios ("failed", "fstat of %s", tmpfil);

    if (delay < 0)
	splitsw = 10;
    else
	splitsw = delay;

    status = 0;

    vec = argsplit(postproc, &program, &vecp);
    if (verbsw)
	vec[vecp++] = "-verbose";

    if ((cp = context_find ("credentials"))) {
	/* post doesn't read context so need to pass credentials. */
	vec[vecp++] = "-credentials";
	vec[vecp++] = cp;
    }

    switch (sendsbr (vec, vecp, program, tmpfil, &st, 0, NULL)) {
	case DONE:
	case NOTOK:
	    status++;
	    break;
	case OK:
	    break;
    }

    fclose (fp);
    if (m_unlink (tmpfil) == -1)
	advise (tmpfil, "unable to remove temp file %s", tmpfil);
    done (status);
    return 1;
}
