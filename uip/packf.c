
/*
 * packf.c -- pack a nmh folder into a file
 *
 * $Id$
 */

#include <h/mh.h>
#include <fcntl.h>
#include <h/dropsbr.h>
#include <errno.h>

/*
 * We allocate space for messages (msgs array)
 * this number of elements at a time.
 */
#define MAXMSGS  256


static struct swit switches[] = {
#define FILESW         0
    { "file name", 0 },
#define MBOXSW         1
    { "mbox", 0 },
#define MMDFSW         2
    { "mmdf", 0 },
#define VERSIONSW      3
    { "version", 0 },
#define	HELPSW         4
    { "help", 4 },
    { NULL, 0 }
};

extern int errno;

static int md = NOTOK;
static int mbx_style = MBOX_FORMAT;
static int mapping = 0;

char *file = NULL;


int
main (int argc, char **argv)
{
    int nummsgs, maxmsgs, fd, msgnum;
    char *cp, *maildir, *msgnam, *folder = NULL, buf[BUFSIZ];
    char **argp, **arguments, **msgs;
    struct msgs *mp;
    struct stat st;

#ifdef LOCALE
    setlocale(LC_ALL, "");
#endif
    invo_name = r1bindex (argv[0], '/');

    /* read user profile/context */
    context_read();

    arguments = getarguments (invo_name, argc, argv, 1);
    argp = arguments;

    /* Allocate the initial space to record message
     * names and ranges.
     */
    nummsgs = 0;
    maxmsgs = MAXMSGS;
    if (!(msgs = (char **) malloc ((size_t) (maxmsgs * sizeof(*msgs)))))
	adios (NULL, "unable to allocate storage");

    /*
     * Parse arguments
     */
    while ((cp = *argp++)) {
	if (*cp == '-') {
	    switch (smatch (++cp, switches)) {
		case AMBIGSW: 
		    ambigsw (cp, switches);
		    done (1);
		case UNKWNSW: 
		    adios (NULL, "-%s unknown", cp);

		case HELPSW: 
		    snprintf (buf, sizeof(buf), "%s [+folder] [msgs] [switches]",
			invo_name);
		    print_help (buf, switches, 1);
		    done (1);
		case VERSIONSW:
		    print_version(invo_name);
		    done (1);

		case FILESW: 
		    if (file)
			adios (NULL, "only one file at a time!");
		    if (!(file = *argp++) || *file == '-')
			adios (NULL, "missing argument to %s", argp[-2]);
		    continue;

		case MBOXSW:
		    mbx_style = MBOX_FORMAT;
		    mapping = 0;
		    continue;
		case MMDFSW:
		    mbx_style = MMDF_FORMAT;
		    mapping = 1;
		    continue;
	    }
	}
	if (*cp == '+' || *cp == '@') {
	    if (folder)
		adios (NULL, "only one folder at a time!");
	    folder = path (cp + 1, *cp == '+' ? TFOLDER : TSUBCWF);
	} else {
	    /*
	     * Check if we need to allocate more space
	     * for message name/ranges.
	     */
	    if (nummsgs >= maxmsgs) {
		maxmsgs += MAXMSGS;
		if (!(msgs = (char **) realloc (msgs,
			(size_t) (maxmsgs * sizeof(*msgs)))))
		    adios (NULL, "unable to reallocate msgs storage");
	    }
	    msgs[nummsgs++] = cp;
	}
    }

    if (!file)
	file = "./msgbox";
    file = path (file, TFILE);

    /*
     * Check if file to be created (or appended to)
     * exists.  If not, ask for confirmation.
     */
    if (stat (file, &st) == NOTOK) {
	if (errno != ENOENT)
	    adios (file, "error on file");
	cp = concat ("Create file \"", file, "\"? ", NULL);
	if (!getanswer (cp))
	    done (1);
	free (cp);
    }

    if (!context_find ("path"))
	free (path ("./", TFOLDER));

    /* default is to pack whole folder */
    if (!nummsgs)
	msgs[nummsgs++] = "all";

    if (!folder)
	folder = getfolder (1);
    maildir = m_maildir (folder);

    if (chdir (maildir) == NOTOK)
	adios (maildir, "unable to change directory to ");

    /* read folder and create message structure */
    if (!(mp = folder_read (folder)))
	adios (NULL, "unable to read folder %s", folder);

    /* check for empty folder */
    if (mp->nummsg == 0)
	adios (NULL, "no messages in %s", folder);

    /* parse all the message ranges/sequences and set SELECTED */
    for (msgnum = 0; msgnum < nummsgs; msgnum++)
	if (!m_convert (mp, msgs[msgnum]))
	    done (1);
    seq_setprev (mp);	/* set the previous-sequence */

    /* open and lock new maildrop file */
    if ((md = mbx_open(file, mbx_style, getuid(), getgid(), m_gmprot())) == NOTOK)
	adios (file, "unable to open");

    /* copy all the SELECTED messages to the file */
    for (msgnum = mp->lowsel; msgnum <= mp->hghsel; msgnum++)
	if (is_selected(mp, msgnum)) {
	    if ((fd = open (msgnam = m_name (msgnum), O_RDONLY)) == NOTOK) {
		admonish (msgnam, "unable to read message");
		break;
	    }

	    if (mbx_copy (file, mbx_style, md, fd, mapping, NULL, 1) == NOTOK)
		adios (file, "error writing to file");

	    close (fd);
	}

    /* close and unlock maildrop file */
    mbx_close (file, md);

    context_replace (pfolder, folder);	/* update current folder         */
    if (mp->hghsel != mp->curmsg)
	seq_setcur (mp, mp->lowsel);
    seq_save (mp);
    context_save ();			/* save the context file         */
    folder_free (mp);			/* free folder/message structure */
    return done (0);
}

int
done (int status)
{
    mbx_close (file, md);
    exit (status);
    return 1;  /* dead code to satisfy the compiler */
}
