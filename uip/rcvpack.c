
/*
 * rcvpack.c -- append message to a file
 *
 * $Id$
 */

#include <h/mh.h>
#include <h/dropsbr.h>
#include <h/rcvmail.h>
#include <h/tws.h>
#include <h/mts.h>

static struct swit switches[] = {
#define MBOXSW       0
    { "mbox", 0 },
#define MMDFSW       1
    { "mmdf", 0 },
#define VERSIONSW    2
    { "version", 0 },
#define	HELPSW       3
    { "help", 0 },
    { NULL, 0 }
};

/*
 * default format in which to save messages
 */
static int mbx_style = MBOX_FORMAT;


int
main (int argc, char **argv)
{
    int md;
    char *cp, *file = NULL, buf[BUFSIZ];
    char **argp, **arguments;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    mts_init (invo_name);
    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /* parse arguments */
    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [switches] file", invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case MBOXSW:
		    mbx_style = MBOX_FORMAT;
		    continue;
		case MMDFSW:
		    mbx_style = MMDF_FORMAT;
		    continue;
	    }
	}
	if (file)
	    adios (NULL, "only one file at a time!");
	else
	    file = cp;
    }

    if (!file)
	adios (NULL, "%s [switches] file", invo_name);

    rewind (stdin);

    /* open and lock the file */
    if ((md = mbx_open (file, mbx_style, getuid(), getgid(), m_gmprot())) == NOTOK)
	done (RCV_MBX);

    /* append the message */
    if (mbx_copy (file, mbx_style, md, fileno(stdin), 1, NULL, 0) == NOTOK) {
	mbx_close (file, md);
	done (RCV_MBX);
    }

    /* close and unlock the file */
    if (mbx_close (file, md) == NOTOK)
	done (RCV_MBX);

    return done (RCV_MOK);
}
