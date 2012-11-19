
/*
 * rcvdist.c -- asynchronously redistribute messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/fmt_scan.h>
#include <h/rcvmail.h>
#include <h/tws.h>
#include <h/mts.h>
#include <h/utils.h>

static struct swit switches[] = {
#define	FORMSW       0
    { "form formfile",  4 },
#define VERSIONSW    1
    { "version", 0 },
#define	HELPSW       2
    { "help", 0 },
    { NULL, 0 }
};

static char backup[BUFSIZ] = "";
static char drft[BUFSIZ] = "";
static char tmpfil[BUFSIZ] = "";

/*
 * prototypes
 */
static void rcvdistout (FILE *, char *, char *);
static void unlink_done (int) NORETURN;


int
main (int argc, char **argv)
{
    pid_t child_id;
    int i, vecp = 1;
    char *addrs = NULL, *cp, *form = NULL, buf[BUFSIZ];
    char **argp, **arguments, *vec[MAXARGS];
    FILE *fp;
    char *tfile = NULL;

    done=unlink_done;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    mts_init (invo_name);
    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    vec[vecp++] = --cp;
		    continue;

		case HELPSW: 
		    snprintf (buf, sizeof(buf),
			"%s [switches] [switches for postproc] address ...",
			invo_name);
		    print_help (buf, switches, 1);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

		case FORMSW: 
		    if (!(form = *argp++) || *form == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;
	    }
	}
	addrs = addrs ? add (cp, add (", ", addrs)) : getcpy (cp);
    }

    if (addrs == NULL)
	adios (NULL, "usage: %s [switches] [switches for postproc] address ...",
	    invo_name);

    umask (~m_gmprot ());

    tfile = m_mktemp2(NULL, invo_name, NULL, &fp);
    if (tfile == NULL) adios("rcvdist", "unable to create temporary file");
    strncpy (tmpfil, tfile, sizeof(tmpfil));

    cpydata (fileno (stdin), fileno (fp), "message", tmpfil);
    fseek (fp, 0L, SEEK_SET);

    tfile = m_mktemp2(NULL, invo_name, NULL, NULL);
    if (tfile == NULL) adios("forw", "unable to create temporary file");
    strncpy (drft, tfile, sizeof(tmpfil));

    rcvdistout (fp, form, addrs);
    fclose (fp);

    if (distout (drft, tmpfil, backup) == NOTOK)
	done (1);

    vec[0] = r1bindex (postproc, '/');
    vec[vecp++] = "-dist";
    vec[vecp++] = drft;
    if ((cp = context_find ("mhlproc"))) {
      vec[vecp++] = "-mhlproc";
      vec[vecp++] = cp;
    }
    vec[vecp] = NULL;

    for (i = 0; (child_id = fork()) == NOTOK && i < 5; i++)
	sleep (5);
    switch (child_id) {
	case NOTOK: 
	    admonish (NULL, "unable to fork");/* fall */
	case OK: 
	    execvp (postproc, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (postproc);
	    _exit (1);

	default: 
	    done (pidXwait(child_id, postproc));
    }

    return 0;  /* dead code to satisfy the compiler */
}

/* very similar to routine in replsbr.c */

#define	SBUFSIZ	256

static int outputlinelen = OUTPUTLINELEN;

static struct format *fmt;

static int dat[5];

static char *addrcomps[] = {
    "from",
    "sender",
    "reply-to",
    "to",
    "cc",
    "bcc",
    "resent-from",
    "resent-sender",
    "resent-reply-to",
    "resent-to",
    "resent-cc",
    "resent-bcc",
    NULL
};


static void
rcvdistout (FILE *inb, char *form, char *addrs)
{
    register int char_read = 0, format_len, i, state;
    register char **ap;
    char *cp, *scanl, name[NAMESZ], tmpbuf[SBUFSIZ];
    register struct comp *cptr;
    FILE *out;

    if (!(out = fopen (drft, "w")))
	adios (drft, "unable to create");

    /* get new format string */
    cp = new_fs (form ? form : rcvdistcomps, NULL, NULL);
    format_len = strlen (cp);
    fmt_compile (cp, &fmt, 1);

    for (ap = addrcomps; *ap; ap++) {
	cptr = fmt_findcomp (*ap);
	if (cptr)
	    cptr->c_type |= CT_ADDR;
    }

    cptr = fmt_findcomp ("addresses");
    if (cptr)
	cptr->c_text = addrs;

    for (state = FLD;;) {
	switch (state = m_getfld (state, name, tmpbuf, SBUFSIZ, inb)) {
	    case FLD: 
	    case FLDPLUS: 
	    	i = fmt_addcomp(name, tmpbuf);
		if (i != -1) {
		    char_read += msg_count;
		    while (state == FLDPLUS) {
			state = m_getfld (state, name, tmpbuf, SBUFSIZ, inb);
			fmt_appendcomp(i, name, tmpbuf);
			char_read += msg_count;
		    }
		}

		while (state == FLDPLUS)
		    state = m_getfld (state, name, tmpbuf, SBUFSIZ, inb);
		break;

	    case LENERR: 
	    case FMTERR: 
	    case BODY: 
	    case FILEEOF: 
		goto finished;

	    default: 
		adios (NULL, "m_getfld() returned %d", state);
	}
    }
finished: ;

    i = format_len + char_read + 256;
    scanl = mh_xmalloc ((size_t) i + 2);
    dat[0] = dat[1] = dat[2] = dat[4] = 0;
    dat[3] = outputlinelen;
    fmt_scan (fmt, scanl, i + 1, i, dat);
    fputs (scanl, out);

    if (ferror (out))
	adios (drft, "error writing");
    fclose (out);

    free (scanl);
    fmt_free(fmt, 1);
}


static void
unlink_done (int status)
{
    if (backup[0])
	unlink (backup);
    if (drft[0])
	unlink (drft);
    if (tmpfil[0])
	unlink (tmpfil);

    exit (status ? RCV_MBX : RCV_MOK);
}
