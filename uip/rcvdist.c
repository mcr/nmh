/* rcvdist.c -- asynchronously redistribute messages
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/context_find.h"
#include "sbr/ambigsw.h"
#include "sbr/pidstatus.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/arglist.h"
#include "sbr/error.h"
#include "h/fmt_scan.h"
#include "h/rcvmail.h"
#include "h/tws.h"
#include "h/mts.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_mktemp.h"

#define RCVDIST_SWITCHES \
    X("form formfile", 4, FORMSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(RCVDIST);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(RCVDIST, switches);
#undef X

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
    int vecp;
    char *addrs = NULL, *cp, *form = NULL, buf[BUFSIZ], *program;
    char **argp, **arguments, **vec;
    FILE *fp;
    char *tfile = NULL;

    if (nmh_init(argv[0], true, false)) { return 1; }

    set_done(unlink_done);

    /*
     * Configure this now, since any unknown switches to rcvdist get
     * sent to postproc
     */

    vec = argsplit(postproc, &program, &vecp);

    mts_init ();
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
			die("missing argument to %s", argp[-2]);
		    continue;
	    }
	}
	addrs = addrs ? add (cp, add (", ", addrs)) : mh_xstrdup(cp);
    }

    if (addrs == NULL)
	die("usage: %s [switches] [switches for postproc] address ...",
	    invo_name);

    umask (~m_gmprot ());

    if ((tfile = m_mktemp2(NULL, invo_name, NULL, &fp)) == NULL) {
	die("unable to create temporary file in %s", get_temp_dir());
    }
    strncpy (tmpfil, tfile, sizeof(tmpfil));

    cpydata (fileno (stdin), fileno (fp), "message", tmpfil);
    fseek (fp, 0L, SEEK_SET);

    if ((tfile = m_mktemp2(NULL, invo_name, NULL, NULL)) == NULL) {
	die("unable to create temporary file in %s", get_temp_dir());
    }
    strncpy (drft, tfile, sizeof(tmpfil));

    rcvdistout (fp, form, addrs);
    fclose (fp);

    if (distout (drft, tmpfil, backup) == NOTOK)
	done (1);

    vec[vecp++] = "-dist";
    if ((cp = context_find ("mhlproc"))) {
      vec[vecp++] = "-mhlproc";
      vec[vecp++] = cp;
    }
    vec[vecp++] = drft;
    vec[vecp] = NULL;

    child_id = fork();
    switch (child_id) {
	case NOTOK: 
            adios("fork", "failed:");

	case OK: 
	    execvp (program, vec);
	    fprintf (stderr, "unable to exec ");
	    perror (postproc);
	    _exit (1);

	default: 
	    done (pidXwait(child_id, postproc));
    }

    return 0;  /* dead code to satisfy the compiler */
}

/* very similar to routine in replsbr.c */

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
    int char_read = 0, format_len, i, state;
    char **ap;
    char *cp, name[NAMESZ], tmpbuf[NMH_BUFSIZ];
    charstring_t scanl;
    struct comp *cptr;
    FILE *out;
    m_getfld_state_t gstate;

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

    gstate = m_getfld_state_init(inb);
    for (;;) {
	int msg_count = sizeof tmpbuf;
	switch (state = m_getfld2(&gstate, name, tmpbuf, &msg_count)) {
	    case FLD: 
	    case FLDPLUS: 
	    	i = fmt_addcomptext(name, tmpbuf);
		if (i != -1) {
		    char_read += msg_count;
		    while (state == FLDPLUS) {
			msg_count = sizeof tmpbuf;
			state = m_getfld2(&gstate, name, tmpbuf, &msg_count);
			fmt_appendcomp(i, name, tmpbuf);
			char_read += msg_count;
		    }
		}

		while (state == FLDPLUS) {
		    msg_count = sizeof tmpbuf;
		    state = m_getfld2(&gstate, name, tmpbuf, &msg_count);
		}
		break;

	    case LENERR: 
	    case FMTERR: 
	    case BODY: 
	    case FILEEOF: 
		goto finished;

	    default: 
		die("m_getfld2() returned %d", state);
	}
    }
finished: ;
    m_getfld_state_destroy (&gstate);

    i = format_len + char_read + 256;
    scanl = charstring_create (i + 2);
    dat[0] = dat[1] = dat[2] = dat[4] = 0;
    dat[3] = outputlinelen;
    fmt_scan (fmt, scanl, i, dat, NULL);
    fputs (charstring_buffer (scanl), out);

    if (ferror (out))
	adios (drft, "error writing");
    fclose (out);

    charstring_free (scanl);
    fmt_free(fmt, 1);
}


static void NORETURN
unlink_done (int status)
{
    if (backup[0])
	(void) m_unlink (backup);
    if (drft[0])
	(void) m_unlink (drft);
    if (tmpfil[0])
	(void) m_unlink (tmpfil);

    exit (status ? RCV_MBX : RCV_MOK);
}
