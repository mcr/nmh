/* rcvpack.c -- append message to a file
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <h/dropsbr.h>
#include <h/rcvmail.h>
#include <h/tws.h>
#include <h/mts.h>

#define RCVPACK_SWITCHES \
    X("mbox", 0, MBOXSW) \
    X("mmdf", 0, MMDFSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(RCVPACK);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(RCVPACK, switches);
#undef X

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

    if (nmh_init(argv[0], 2)) { return 1; }

    mts_init ();
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
		    done (0);
		case VERSIONSW:
		    print_version(invo_name);
		    done (0);

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

    done (RCV_MOK);
    return 1;
}
