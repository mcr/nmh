/* prompter.c -- simple prompting editor front-end
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/cpydata.h"
#include "sbr/m_atoi.h"
#include "sbr/context_save.h"
#include "sbr/ambigsw.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/error.h"
#include <fcntl.h>
#include "h/signals.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_mktemp.h"
#include <setjmp.h>

#include <termios.h>

#define	QUOTE '\\'

#define PROMPTER_SWITCHES \
    X("erase chr", 0, ERASESW) \
    X("kill chr", 0, KILLSW) \
    X("prepend", 0, PREPSW) \
    X("noprepend", 0, NPREPSW) \
    X("rapid", 0, RAPDSW) \
    X("norapid", 0, NRAPDSW) \
    X("body", -4, BODYSW) \
    X("nobody", -6, NBODYSW) \
    X("doteof", 0, DOTSW) \
    X("nodoteof", 0, NDOTSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(PROMPTER);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(PROMPTER, switches);
#undef X

static struct termios tio;

static bool wtuser;
static bool sigint;
static jmp_buf sigenv;

/*
 * prototypes
 */
static int getln (char *, int);
static int chrcnv (char *);
static void chrdsp (char *, char);
static void intrser (int);


int
main (int argc, char **argv)
{
    bool body = true;
    bool prepend = true;
    bool rapid = false;
    bool doteof = false;
    int fdi, fdo, i, state;
    char *cp, *drft = NULL, *erasep = NULL;
    char *killp = NULL, name[NAMESZ], field[NMH_BUFSIZ];
    char buffer[BUFSIZ];
    char **arguments, **argp;
    FILE *in, *out;
    char *tmpfil;
    m_getfld_state_t gstate;

    if (nmh_init(argv[0], true, false)) { return 1; }

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    while ((cp = *argp++))
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    die("-%s unknown", cp);

		case HELPSW: 
		    snprintf (buffer, sizeof(buffer), "%s [switches] file",
			invo_name);
		    print_help (buffer, switches, 1);
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

		case ERASESW: 
		    if (!(erasep = *argp++) || *erasep == '-')
			die("missing argument to %s", argp[-2]);
		    continue;
		case KILLSW: 
		    if (!(killp = *argp++) || *killp == '-')
			die("missing argument to %s", argp[-2]);
		    continue;

		case PREPSW: 
		    prepend = true;
		    continue;
		case NPREPSW: 
		    prepend = false;
		    continue;

		case RAPDSW: 
		    rapid = true;
		    continue;
		case NRAPDSW: 
		    rapid = false;
		    continue;

		case BODYSW: 
		    body = true;
		    continue;
		case NBODYSW: 
		    body = false;
		    continue;

		case DOTSW: 
		    doteof = true;
		    continue;
		case NDOTSW: 
		    doteof = false;
		    continue;
	    }
	} else {
	    if (!drft)
		drft = cp;
	}

    if (!drft)
	die("usage: %s [switches] file", invo_name);
    if ((in = fopen (drft, "r")) == NULL)
	adios (drft, "unable to open");

    if ((tmpfil = m_mktemp2(NULL, invo_name, NULL, &out)) == NULL) {
	die("unable to create temporary file in %s", get_temp_dir());
    }

    /*
     * Are we changing the kill or erase character?
     */
    if (killp || erasep) {
	cc_t save_erase, save_kill;

	/* get the current terminal attributes */
	tcgetattr(0, &tio);

	/* save original kill, erase character for later */
	save_kill = tio.c_cc[VKILL];
	save_erase = tio.c_cc[VERASE];

	/* set new kill, erase character in terminal structure */
	tio.c_cc[VKILL] = killp ? chrcnv (killp) : save_kill;
	tio.c_cc[VERASE] = erasep ? chrcnv (erasep) : save_erase;

	/* set the new terminal attributes */
	 tcsetattr(0, TCSADRAIN, &tio);

	/* print out new kill erase characters */
	chrdsp ("erase", tio.c_cc[VERASE]);
	chrdsp (", kill", tio.c_cc[VKILL]);
	chrdsp (", intr", tio.c_cc[VINTR]);
	putchar ('\n');
	fflush (stdout);

	/*
	 * We set the kill and erase character back to original
	 * setup in terminal structure so we can easily
	 * restore it upon exit.
	 */
	tio.c_cc[VKILL] = save_kill;
	tio.c_cc[VERASE] = save_erase;
    }

    sigint = false;
    SIGNAL2 (SIGINT, intrser);

    /*
     * Loop through the lines of the draft skeleton.
     */
    gstate = m_getfld_state_init(in);
    for (;;) {
	int fieldsz = sizeof field;
	switch (state = m_getfld2(&gstate, name, field, &fieldsz)) {
	    case FLD: 
	    case FLDPLUS: 
		/*
		 * Check if the value of field contains anything
		 * other than space or tab.
		 */
		for (cp = field; *cp; cp++)
		    if (*cp != ' ' && *cp != '\t')
			break;

		/* If so, just add header line to draft */
		if (*cp++ != '\n' || *cp != 0) {
		    printf ("%s:%s", name, field);
		    fprintf (out, "%s:%s", name, field);
		    while (state == FLDPLUS) {
			fieldsz = sizeof field;
			state = m_getfld2(&gstate, name, field, &fieldsz);
			fputs(field, stdout);
			fputs(field, out);
		    }
		} else {
		    /* Else, get value of header field */
		    printf ("%s: ", name);
		    fflush (stdout);
		    i = getln (field, sizeof(field));
		    if (i == -1) {
abort:
			if (killp || erasep) {
			    tcsetattr(0, TCSADRAIN, &tio);
			}
			(void) m_unlink (tmpfil);
			done (1);
		    }
		    if (i != 0 || (field[0] != '\n' && field[0] != 0)) {
			fprintf (out, "%s:", name);
			do {
			    if (field[0] != ' ' && field[0] != '\t')
				putc (' ', out);
			    fputs(field, out);
			} while (i == 1
				    && (i = getln (field, sizeof(field))) >= 0);
			if (i == -1)
			    goto abort;
		    }
		}

		continue;

	    case BODY: 
	    case FILEEOF: 
	        if (!body)
	            break;
		fprintf (out, "--------\n");
		if (field[0] == 0 || !prepend)
		    puts("--------");
		if (field[0]) {
		    if (prepend && body) {
			puts("\n--------Enter initial text\n");
			fflush (stdout);
			for (;;) {
			    getln (buffer, sizeof(buffer));
			    if (doteof && buffer[0] == '.' && buffer[1] == '\n')
				break;
			    if (buffer[0] == 0)
				break;
			    fputs(buffer, out);
			}
		    }

		    do {
			fputs(field, out);
			if (!rapid && !sigint)
			    fputs(field, stdout);
		    } while (state == BODY &&
			    (fieldsz = sizeof field,
			     state = m_getfld2(&gstate, name, field, &fieldsz)));
		    if (prepend || !body)
			break;
                    puts("\n--------Enter additional text\n");
		}

		fflush (stdout);
		for (;;) {
		    getln (field, sizeof(field));
		    if (doteof && field[0] == '.' && field[1] == '\n')
			break;
		    if (field[0] == 0)
			break;
		    fputs(field, out);
		}
		break;

	    default: 
		die("skeleton is poorly formatted");
	}
	break;
    }
    m_getfld_state_destroy (&gstate);

    if (body)
	puts("--------");

    fflush (stdout);
    fclose (in);
    fclose (out);
    SIGNAL (SIGINT, SIG_IGN);

    if (killp || erasep) {
	 tcsetattr(0, TCSADRAIN, &tio);
    }

    if ((fdi = open (tmpfil, O_RDONLY)) == NOTOK)
	adios (tmpfil, "unable to re-open");
    if ((fdo = creat (drft, m_gmprot ())) == NOTOK)
	adios (drft, "unable to write");
    cpydata (fdi, fdo, tmpfil, drft);
    close (fdi);
    close (fdo);
    (void) m_unlink (tmpfil);

    context_save ();	/* save the context file */
    done (0);
    return 1;
}


static int
getln (char *buffer, int n)
{
    int c;
    char *cp;
    static bool quoting;

    *buffer = 0;

    switch (setjmp (sigenv)) {
	case OK: 
	    wtuser = true;
	    break;

	case DONE: 
	    wtuser = false;
	    return 0;

	default: 
	    wtuser = false;
	    return NOTOK;
    }

    cp = buffer;
    *cp = 0;

    for (;;) {
	switch (c = getchar ()) {
	    case EOF: 
		quoting = false;
		clearerr (stdin);
		longjmp (sigenv, DONE);

	    case '\n': 
		if (quoting) {
		    *(cp - 1) = c;
		    quoting = false;
		    wtuser = false;
		    return 1;
		}
		*cp++ = c;
		*cp = 0;
		wtuser = false;
		return 0;

	    default: 
		if (c == QUOTE) {
		    quoting = true;
		} else {
		    quoting = false;
		}
		if (cp < buffer + n)
		    *cp++ = c;
		*cp = 0;
	}
    }
}


static void
intrser (int i)
{
    NMH_UNUSED (i);

    if (wtuser)
	longjmp (sigenv, NOTOK);
    sigint = true;
}


static int
chrcnv (char *cp)
{
    return *cp != QUOTE ? *cp : m_atoi(++cp);
}


static void
chrdsp (char *s, char c)
{
    printf ("%s ", s);
    if (c < ' ' || c == 0177)
	printf ("^%c", c ^ 0100);
    else
	printf ("%c", c);
}
