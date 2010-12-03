
/*
 * rcvstore.c -- asynchronously add mail to a folder
 *
 * This code is Copyright (c) 2002, by the authors of nmh.  See the
 * COPYRIGHT file in the root directory of the nmh distribution for
 * complete copyright information.
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/signals.h>
#include <errno.h>
#include <signal.h>
#include <h/mts.h>

static struct swit switches[] = {
#define CRETSW         0
    { "create",	0 },
#define NCRETSW        1
    { "nocreate", 0 },
#define UNSEENSW       2
    { "unseen", 0 },
#define NUNSEENSW      3
    { "nounseen", 0 },
#define PUBSW          4
    { "public",	0 },
#define NPUBSW         5
    { "nopublic",  0 },
#define ZEROSW         6
    { "zero",	0 },
#define NZEROSW        7
    { "nozero",	0 },
#define SEQSW          8
    { "sequence name", 0 },
#define VERSIONSW      9
    { "version", 0 },
#define HELPSW        10
    { "help", 0 },
    { NULL, 0 }
};


/*
 * name of temporary file to store incoming message
 */
static char *tmpfilenam = NULL;

static void unlink_done(int) NORETURN;

int
main (int argc, char **argv)
{
    int publicsw = -1, zerosw = 0;
    int create = 1, unseensw = 1;
    int fd, msgnum, seqp = 0;
    char *cp, *maildir, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments, *seqs[NUMATTRS+1];
    struct msgs *mp;
    struct stat st;

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
		snprintf (buf, sizeof(buf), "%s [+folder] [switches]",
			  invo_name);
		print_help (buf, switches, 1);
		done (1);
	    case VERSIONSW:
		print_version(invo_name);
		done (1);

	    case SEQSW: 
		if (!(cp = *argp++) || *cp == '-')
		    adios (NULL, "missing argument name to %s", argp[-2]);

		/* check if too many sequences specified */
		if (seqp >= NUMATTRS)
		    adios (NULL, "too many sequences (more than %d) specified", NUMATTRS);
		seqs[seqp++] = cp;
		continue;

	    case UNSEENSW:
		unseensw = 1;
		continue;
	    case NUNSEENSW:
		unseensw = 0;
		continue;

	    case PUBSW: 
		publicsw = 1;
		continue;
	    case NPUBSW: 
		publicsw = 0;
		continue;

	    case ZEROSW: 
		zerosw++;
		continue;
	    case NZEROSW: 
		zerosw = 0;
		continue;

	    case CRETSW: 
		create++;
		continue;
	    case NCRETSW: 
		create = 0;
		continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    else
		folder = pluspath (cp);
	} else {
	    adios (NULL, "usage: %s [+folder] [switches]", invo_name);
	}
    }

    seqs[seqp] = NULL;	/* NULL terminate list of sequences */

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
	    adios (NULL, "folder %s doesn't exist", maildir);
	if (!makedir (maildir))
	    adios (NULL, "unable to create folder %s", maildir);
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
	adios ("rcvstore", "unable to create temporary file");
    }
    chmod (tmpfilenam, m_gmprot());

    /* copy the message from stdin into temp file */
    cpydata (fileno (stdin), fd, "standard input", tmpfilenam);

    if (fstat (fd, &st) == NOTOK) {
	unlink (tmpfilenam);
	adios (tmpfilenam, "unable to fstat");
    }
    if (close (fd) == NOTOK)
	adios (tmpfilenam, "error closing");

    /* don't add file if it is empty */
    if (st.st_size == 0) {
	unlink (tmpfilenam);
	advise (NULL, "empty file");
	done (0);
    }

    /*
     * read folder and create message structure
     */
    if (!(mp = folder_read (folder)))
	adios (NULL, "unable to read folder %s", folder);

    /*
     * Link message into folder, and possibly add
     * to the Unseen-Sequence's.
     */
    if ((msgnum = folder_addmsg (&mp, tmpfilenam, 0, unseensw, 0, 0, (char *)0)) == -1)
	done (1);

    /*
     * Add the message to any extra sequences
     * that have been specified.
     */
    for (seqp = 0; seqs[seqp]; seqp++) {
	if (!seq_addmsg (mp, seqs[seqp], msgnum, publicsw, zerosw))
	    done (1);
    }

    seq_setunseen (mp, 0);	/* synchronize any Unseen-Sequence's      */
    seq_save (mp);		/* synchronize and save message sequences */
    folder_free (mp);		/* free folder/message structure          */

    context_save ();		/* save the global context file           */
    unlink (tmpfilenam);	/* remove temporary file                  */
    tmpfilenam = NULL;

    done (0);
    return 1;
}

/*
 * Clean up and exit
 */
static void
unlink_done(int status)
{
    if (tmpfilenam && *tmpfilenam)
	unlink (tmpfilenam);
    exit (status);
}
