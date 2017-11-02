/* rcvstore.c -- asynchronously add mail to a folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include "h/mh.h"
#include "sbr/print_version.h"
#include "sbr/print_help.h"
#include "sbr/seq_add.h"
#include "sbr/error.h"
#include <fcntl.h>
#include "h/signals.h"
#include "h/mts.h"
#include "h/done.h"
#include "h/utils.h"
#include "sbr/m_maildir.h"
#include "sbr/m_mktemp.h"
#include "sbr/makedir.h"

#define RCVSTORE_SWITCHES \
    X("create", 0, CRETSW) \
    X("nocreate", 0, NCRETSW) \
    X("unseen", 0, UNSEENSW) \
    X("nounseen", 0, NUNSEENSW) \
    X("public", 0, PUBSW) \
    X("nopublic", 0, NPUBSW) \
    X("zero", 0, ZEROSW) \
    X("nozero", 0, NZEROSW) \
    X("sequence name", 0, SEQSW) \
    X("version", 0, VERSIONSW) \
    X("help", 0, HELPSW) \

#define X(sw, minchars, id) id,
DEFINE_SWITCH_ENUM(RCVSTORE);
#undef X

#define X(sw, minchars, id) { sw, minchars, id },
DEFINE_SWITCH_ARRAY(RCVSTORE, switches);
#undef X


/*
 * name of temporary file to store incoming message
 */
static char *tmpfilenam = NULL;

static void unlink_done(int) NORETURN;

int
main (int argc, char **argv)
{
    int publicsw = -1;
    bool zerosw = false;
    bool create = true;
    bool unseensw = true;
    int fd, msgnum;
    size_t seqp = 0;
    char *cp, *maildir, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments;
    svector_t seqs = svector_create (0);
    struct msgs *mp;
    struct stat st;

    if (nmh_init(argv[0], true, false)) { return 1; }

    set_done(unlink_done);

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
		die("-%s unknown", cp);

	    case HELPSW: 
		snprintf (buf, sizeof(buf), "%s [+folder] [switches]",
			  invo_name);
		print_help (buf, switches, 1);
		done (0);
	    case VERSIONSW:
		print_version(invo_name);
		done (0);

	    case SEQSW: 
		if (!(cp = *argp++) || *cp == '-')
		    die("missing argument name to %s", argp[-2]);

		svector_push_back (seqs, cp);
		seqp++;
		continue;

	    case UNSEENSW:
		unseensw = true;
		continue;
	    case NUNSEENSW:
		unseensw = false;
		continue;

	    case PUBSW: 
		publicsw = 1;
		continue;
	    case NPUBSW: 
		publicsw = 0;
		continue;

	    case ZEROSW: 
		zerosw = true;
		continue;
	    case NZEROSW: 
		zerosw = false;
		continue;

	    case CRETSW: 
		create = true;
		continue;
	    case NCRETSW: 
		create = false;
		continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		die("only one folder at a time!");
            folder = pluspath (cp);
	} else {
	    die("usage: %s [+folder] [switches]", invo_name);
	}
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    /* if no folder is given, use default folder */
    if (!folder)
	folder = getfolder (0);
    maildir = m_maildir (folder);

    /* check if folder exists */
    if (stat (maildir, &st) == NOTOK) {
	if (errno != ENOENT)
	    adios (maildir, "error on folder");
	if (!create)
	    die("folder %s doesn't exist", maildir);
	if (!makedir (maildir))
	    die("unable to create folder %s", maildir);
    }

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to");

    /* ignore a few signals */
    SIGNAL (SIGHUP, SIG_IGN);
    SIGNAL (SIGINT, SIG_IGN);
    SIGNAL (SIGQUIT, SIG_IGN);
    SIGNAL (SIGTERM, SIG_IGN);

    /* create a temporary file */
    tmpfilenam = m_mktemp (invo_name, &fd, NULL);
    if (tmpfilenam == NULL) {
	die("unable to create temporary file in %s", get_temp_dir());
    }
    chmod (tmpfilenam, m_gmprot());

    /* copy the message from stdin into temp file */
    cpydata (fileno (stdin), fd, "standard input", tmpfilenam);

    if (fstat (fd, &st) == NOTOK) {
	(void) m_unlink (tmpfilenam);
	adios (tmpfilenam, "unable to fstat");
    }
    if (close (fd) == NOTOK)
	adios (tmpfilenam, "error closing");

    /* don't add file if it is empty */
    if (st.st_size == 0) {
	(void) m_unlink (tmpfilenam);
	inform("empty file");
	done (0);
    }

    /*
     * read folder and create message structure
     */
    if (!(mp = folder_read (folder, 1)))
	die("unable to read folder %s", folder);

    /*
     * Link message into folder, and possibly add
     * to the Unseen-Sequence's.
     */
    if ((msgnum = folder_addmsg (&mp, tmpfilenam, 0, unseensw, 0, 0, NULL)) == -1)
	done (1);

    /*
     * Add the message to any extra sequences
     * that have been specified.
     */
    if (seqp) {
	/* The only reason that seqp was checked to be non-zero is in
	   case a -nosequence switch is added. */
	for (seqp = 0; seqp < svector_size (seqs); seqp++) {
	    if (!seq_addmsg (mp, svector_at (seqs, seqp), msgnum, publicsw,
			     zerosw))
		done (1);
	}
    }

    svector_free (seqs);
    seq_setunseen (mp, 0);	/* synchronize any Unseen-Sequence's      */
    seq_save (mp);		/* synchronize and save message sequences */
    folder_free (mp);		/* free folder/message structure          */

    context_save ();		/* save the global context file           */
    (void) m_unlink (tmpfilenam); /* remove temporary file                  */
    tmpfilenam = NULL;

    done (0);
    return 1;
}

/*
 * Clean up and exit
 */
static void NORETURN
unlink_done(int status)
{
    if (tmpfilenam && *tmpfilenam)
	(void) m_unlink (tmpfilenam);
    exit (status);
}
