
/*
 * rcvdist.c -- asynchronously redistribute messages
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/fmt_scan.h>
#include <h/rcvmail.h>
#include <h/tws.h>
#include <h/mts.h>

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
int done (int);


int
main (int argc, char **argv)
{
    pid_t child_id;
    int i, vecp = 1;
    char *addrs = NULL, *cp, *form = NULL, buf[BUFSIZ];
    char **argp, **arguments, *vec[MAXARGS];
    register FILE *fp;

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
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

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
    strncpy (tmpfil, m_tmpfil (invo_name), sizeof(tmpfil));
    if ((fp = fopen (tmpfil, "w+")) == NULL)
	adios (tmpfil, "unable to create");
    cpydata (fileno (stdin), fileno (fp), "message", tmpfil);
    fseek (fp, 0L, SEEK_SET);
    strncpy (drft, m_tmpfil (invo_name), sizeof(drft));
    rcvdistout (fp, form, addrs);
    fclose (fp);

    if (distout (drft, tmpfil, backup) == NOTOK)
	done (1);

    vec[0] = r1bindex (postproc, '/');
    vec[vecp++] = "-dist";
    vec[vecp++] = drft;
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

static int ncomps = 0;
static char **compbuffers = 0;
static struct comp **used_buf = 0;

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
    register char *tmpbuf, **nxtbuf, **ap;
    char *cp, *scanl, name[NAMESZ];
    register struct comp *cptr, **savecomp;
    FILE *out;

    if (!(out = fopen (drft, "w")))
	adios (drft, "unable to create");

    /* get new format string */
    cp = new_fs (form ? form : rcvdistcomps, NULL, NULL);
    format_len = strlen (cp);
    ncomps = fmt_compile (cp, &fmt) + 1;
    if (!(nxtbuf = compbuffers = (char **) calloc ((size_t) ncomps, sizeof(char *))))
	adios (NULL, "unable to allocate component buffers");
    if (!(savecomp = used_buf = (struct comp **) calloc ((size_t) (ncomps + 1), sizeof(struct comp *))))
	adios (NULL, "unable to allocate component buffer stack");
    savecomp += ncomps + 1;
    *--savecomp = 0;

    for (i = ncomps; i--;)
	if (!(*nxtbuf++ = malloc (SBUFSIZ)))
	    adios (NULL, "unable to allocate component buffer");
    nxtbuf = compbuffers;
    tmpbuf = *nxtbuf++;

    for (ap = addrcomps; *ap; ap++) {
	FINDCOMP (cptr, *ap);
	if (cptr)
	    cptr->c_type |= CT_ADDR;
    }

    FINDCOMP (cptr, "addresses");
    if (cptr)
	cptr->c_text = addrs;

    for (state = FLD;;) {
	switch (state = m_getfld (state, name, tmpbuf, SBUFSIZ, inb)) {
	    case FLD: 
	    case FLDPLUS: 
		if ((cptr = wantcomp[CHASH (name)]))
		    do {
			if (!strcasecmp (name, cptr->c_name)) {
			    char_read += msg_count;
			    if (!cptr->c_text) {
				cptr->c_text = tmpbuf;
				*--savecomp = cptr;
				tmpbuf = *nxtbuf++;
			    }
			    else {
				i = strlen (cp = cptr->c_text) - 1;
				if (cp[i] == '\n') {
				    if (cptr->c_type & CT_ADDR) {
					cp[i] = 0;
					cp = add (",\n\t", cp);
				    }
				    else
					cp = add ("\t", cp);
				}
				cptr->c_text = add (tmpbuf, cp);
			    }
			    while (state == FLDPLUS) {
				state = m_getfld (state, name, tmpbuf,
						  SBUFSIZ, inb);
				cptr->c_text = add (tmpbuf, cptr->c_text);
				char_read += msg_count;
			    }
			    break;
			}
		    } while ((cptr = cptr->c_next));

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
    scanl = malloc ((size_t) i + 2);
    dat[0] = dat[1] = dat[2] = dat[4] = 0;
    dat[3] = outputlinelen;
    fmt_scan (fmt, scanl, i, dat);
    fputs (scanl, out);

    if (ferror (out))
	adios (drft, "error writing");
    fclose (out);

    free (scanl);
    for (nxtbuf = compbuffers, i = ncomps; (cptr = *savecomp++); nxtbuf++, i--)
	free (cptr->c_text);
    while (i-- > 0)
        free (*nxtbuf++);
    free ((char *) compbuffers);
    free ((char *) used_buf);
}


int
done (int status)
{
    if (backup[0])
	unlink (backup);
    if (drft[0])
	unlink (drft);
    if (tmpfil[0])
	unlink (tmpfil);

    exit (status ? RCV_MBX : RCV_MOK);
    return 1;  /* dead code to satisfy the compiler */
}
